#include "Session.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace terminal_example {
namespace {

class Handle {
public:
    Handle() = default;
    explicit Handle(HANDLE value) : value_(value) {}
    ~Handle() { reset(); }
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    HANDLE *put() { reset(); return &value_; }
    HANDLE get() const { return value_; }
    HANDLE release() { return std::exchange(value_, nullptr); }
    void reset(HANDLE value = nullptr) {
        if (value_) CloseHandle(value_);
        value_ = value;
    }
private:
    HANDLE value_ = nullptr;
};

struct PseudoConsoleApi {
    decltype(&CreatePseudoConsole) create = nullptr;
    decltype(&ResizePseudoConsole) resize = nullptr;
    decltype(&ClosePseudoConsole) close = nullptr;
};

template <typename Fn>
Fn consoleProc(HMODULE module, const char *name) {
    // A Windows API export has the signature provided by the SDK.
    const auto proc = GetProcAddress(module, name);
    Fn result = nullptr;
    static_assert(sizeof(result) == sizeof(proc));
    std::memcpy(&result, &proc, sizeof(result));
    return result;
}

const PseudoConsoleApi &api() {
    static const auto functions = [] {
        const auto kernel = GetModuleHandleW(L"kernel32.dll");
        return PseudoConsoleApi{consoleProc<decltype(&CreatePseudoConsole)>(kernel, "CreatePseudoConsole"),
            consoleProc<decltype(&ResizePseudoConsole)>(kernel, "ResizePseudoConsole"),
            consoleProc<decltype(&ClosePseudoConsole)>(kernel, "ClosePseudoConsole")};
    }();
    return functions;
}

COORD consoleSize(cyui::TerminalSize size) {
    return {SHORT(std::clamp(size.columns, 2, 300)), SHORT(std::clamp(size.rows, 2, 150))};
}

struct AttributeList {
    explicit AttributeList(SIZE_T bytes) : storage(bytes) {}
    ~AttributeList() { if (list) DeleteProcThreadAttributeList(list); }
    std::vector<unsigned char> storage;
    LPPROC_THREAD_ATTRIBUTE_LIST list = nullptr;
};

} // namespace

struct Session::Impl {
    HPCON console = nullptr;
    Handle output, input, process, job;
    std::string error;
    std::thread writer;
    std::mutex mutex;
    std::condition_variable ready;
    std::string pending;
    COORD requested{};
    bool stopping = false, writeFailed = false;
    std::chrono::steady_clock::time_point lastOutput;

    void writeLoop() {
        for (;;) {
            std::string bytes;
            COORD dimensions{};
            {
                std::unique_lock<std::mutex> lock(mutex);
                ready.wait(lock, [&] { return stopping || requested.X || !pending.empty(); });
                if (stopping) return;
                dimensions = std::exchange(requested, COORD{});
                bytes.swap(pending);
            }
            if (dimensions.X && FAILED(api().resize(console, dimensions))) {
                std::lock_guard<std::mutex> lock(mutex);
                writeFailed = true;
                return;
            }
            for (std::size_t sent = 0; sent < bytes.size();) {
                DWORD count = 0;
                if (!WriteFile(input.get(), bytes.data() + sent, DWORD(bytes.size() - sent), &count, nullptr) || !count) {
                    std::lock_guard<std::mutex> lock(mutex);
                    writeFailed = true;
                    return;
                }
                sent += count;
            }
        }
    }
};

Session::Session() : impl_(std::make_unique<Impl>()) {}
Session::~Session() { stop(); }

const char *const *Session::shells() {
    static const char *const labels[]{"PowerShell", "Command Prompt"};
    return labels;
}

bool Session::start(int shell, cyui::TerminalSize size) {
    stop();
    auto &s = *impl_;
    s.error.clear();
    if (!api().create || !api().close || !api().resize) {
        s.error = "This Windows version does not provide a pseudoconsole.";
        return false;
    }
    Handle childInput, childOutput;
    const auto fail = [&](const char *message, DWORD code) {
        // Release the local child ends before tearing down ConPTY.
        childInput.reset();
        childOutput.reset();
        stop();
        s.error = std::string(message) + " (" + std::to_string(code) + ").";
        return false;
    };
    if (!CreatePipe(childInput.put(), s.input.put(), nullptr, 0) ||
        !CreatePipe(s.output.put(), childOutput.put(), nullptr, 0))
        return fail("Could not create terminal pipes", GetLastError());
    const auto result = api().create(consoleSize(size), childInput.get(), childOutput.get(), 0, &s.console);
    if (FAILED(result)) return fail("Could not create the Windows pseudoconsole", DWORD(result));

    wchar_t system[MAX_PATH];
    const auto length = GetSystemDirectoryW(system, MAX_PATH);
    if (!length || length >= MAX_PATH) return fail("Could not locate Windows system tools", GetLastError());
    std::wstring executable(system, length);
    executable += shell == 1 ? L"\\cmd.exe" : L"\\WindowsPowerShell\\v1.0\\powershell.exe";
    std::wstring command = L"\"" + executable + L"\"" + (shell == 1 ? L" /D" : L" -NoLogo");

    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    AttributeList attributes(bytes);
    const auto list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.storage.data());
    if (!InitializeProcThreadAttributeList(list, 1, 0, &bytes))
        return fail("Could not initialize shell attributes", GetLastError());
    attributes.list = list;
    if (!UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                   s.console, sizeof(s.console), nullptr, nullptr))
        return fail("Could not attach the shell to the pseudoconsole", GetLastError());
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    // Null std handles let ConPTY supply them even if this host was launched with redirected streams.
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.lpAttributeList = list;
    PROCESS_INFORMATION info{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
                        EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED, nullptr, nullptr, &startup.StartupInfo, &info))
        return fail("Could not start the shell", GetLastError());
    s.process.reset(info.hProcess);
    Handle thread(info.hThread);
    childInput.reset();
    childOutput.reset();

    // When Windows allows nesting jobs, clean up the entire process tree on stop.
    s.job.reset(CreateJobObjectW(nullptr, nullptr));
    if (s.job.get()) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(s.job.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits)) ||
            !AssignProcessToJobObject(s.job.get(), s.process.get())) s.job.reset();
    }
    if (ResumeThread(thread.get()) == DWORD(-1)) return fail("Could not resume the shell", GetLastError());
    s.lastOutput = std::chrono::steady_clock::now();
    try {
        s.writer = std::thread([&s] { s.writeLoop(); });
    } catch (const std::system_error &e) {
        return fail("Could not start terminal input", DWORD(e.code().value()));
    }
    return true;
}

void Session::stop() {
    auto &s = *impl_;
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        s.stopping = true;
    }
    s.ready.notify_one();
    s.job.reset();
    if (s.process.get() && WaitForSingleObject(s.process.get(), 0) == WAIT_TIMEOUT)
        TerminateProcess(s.process.get(), 1);
    // Closing the read end releases ConPTY even if the output pipe is full.
    s.output.reset();
    if (s.writer.joinable()) {
        // Cancel blocked writes before closing the console. Retry if shutdown raced a new write.
        do {
            CancelSynchronousIo(s.writer.native_handle());
        } while (WaitForSingleObject(s.writer.native_handle(), 10) == WAIT_TIMEOUT);
        s.writer.join();
    }
    if (s.console) api().close(s.console);
    s.console = nullptr;
    s.input.reset();
    s.process.reset();
    s.pending.clear();
    s.requested = {};
    s.stopping = s.writeFailed = false;
}

bool Session::running() const { return impl_->process.get() != nullptr; }

void Session::resize(cyui::TerminalSize size) {
    if (!running()) return;
    auto &s = *impl_;
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        s.requested = consoleSize(size);
    }
    s.ready.notify_one();
}

bool Session::write(std::string_view bytes) {
    if (!running() || bytes.empty()) return false;
    auto &s = *impl_;
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        if (s.writeFailed || bytes.size() > 1024*1024 - s.pending.size()) return false;
        s.pending.append(bytes);
    }
    s.ready.notify_one();
    return true;
}

std::string Session::read() {
    if (!running()) return {};
    auto &s = *impl_;
    std::string text;
    char buffer[8192];
    DWORD available = 0;
    while (text.size() < 128*1024 && PeekNamedPipe(s.output.get(), nullptr, 0, nullptr, &available, nullptr) && available) {
        DWORD read = 0;
        if (!ReadFile(s.output.get(), buffer, std::min<DWORD>(available, sizeof(buffer)), &read, nullptr) || !read)
            break;
        text.append(buffer, read);
    }
    const auto now = std::chrono::steady_clock::now();
    if (!text.empty()) s.lastOutput = now;
    if (WaitForSingleObject(s.process.get(), 0) == WAIT_OBJECT_0 &&
        now - s.lastOutput > std::chrono::milliseconds(120)) {
        DWORD exitCode = 0;
        if (GetExitCodeProcess(s.process.get(), &exitCode) && exitCode != 0)
            s.error = "Shell exit code: " + std::to_string(exitCode);
        stop();
    } else {
        bool failed;
        {
            std::lock_guard<std::mutex> lock(s.mutex);
            failed = s.writeFailed;
        }
        if (failed && WaitForSingleObject(s.process.get(), 0) == WAIT_TIMEOUT) {
            s.error = "The Windows terminal connection closed.";
            stop();
        }
    }
    return text;
}

const std::string &Session::error() const { return impl_->error; }

} // namespace terminal_example
