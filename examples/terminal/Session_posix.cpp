#include "Session.h"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#ifdef __APPLE__
#include <crt_externs.h>
#include <util.h>
#else
#include <pty.h>
#include <sys/syscall.h>
extern char **environ;
#endif

namespace terminal_example {
namespace {

winsize windowSize(cyui::TerminalSize size) {
    winsize result{};
    result.ws_col = static_cast<unsigned short>(std::clamp(size.columns, 2, 300));
    result.ws_row = static_cast<unsigned short>(std::clamp(size.rows, 2, 150));
    return result;
}

char **environment() {
#ifdef __APPLE__
    return *_NSGetEnviron();
#else
    return environ;
#endif
}

void closeInherited(int last) {
#ifdef SYS_close_range
    if (syscall(SYS_close_range, 3u, ~0u, 0u) == 0) return;
#endif
    for (int fd = 3; fd < last; ++fd) close(fd);
}

} // namespace

struct Session::Impl {
    int fd = -1;
    pid_t child = -1;
    std::string pending, error;
};

Session::Session() : impl_(std::make_unique<Impl>()) {}
Session::~Session() { stop(); }

const char *const *Session::shells() {
    static const char *const labels[]{"Default shell", "Bash"};
    return labels;
}

bool Session::start(int shell, cyui::TerminalSize size) {
    stop();
    auto &s = *impl_;
    s.error.clear();
    if (shell == 1 && access("/bin/bash", X_OK) != 0) {
        s.error = "Bash was not found at /bin/bash.";
        return false;
    }
    const char *configured = std::getenv("SHELL");
    std::string executable = shell == 1 ? "/bin/bash" :
        configured && configured[0] == '/' && access(configured, X_OK) == 0 ? configured : "/bin/sh";
    std::string name = executable.substr(executable.find_last_of('/') + 1);
    char interactive[] = "-i";
    char *args[] = {name.data(), interactive, nullptr};
    char terminal[] = "TERM=xterm-256color";
    std::vector<char *> env;
    for (auto entry = environment(); *entry; ++entry) {
        if (std::strncmp(*entry, "TERM=", 5) != 0) env.push_back(*entry);
    }
    env.push_back(terminal);
    env.push_back(nullptr);
    const char *program = executable.c_str();
    auto *const childEnvironment = env.data();
    const long maxFiles = sysconf(_SC_OPEN_MAX);
    const int descriptorLimit = maxFiles > 0 ? int(std::min(maxFiles, 1048576L)) : 65536;
    auto dimensions = windowSize(size);
    s.child = forkpty(&s.fd, nullptr, nullptr, &dimensions);
    if (s.child == 0) {
        closeInherited(descriptorLimit);
        execve(program, args, childEnvironment);
        constexpr char message[] = "Could not execute the shell.\r\n";
        ::write(STDERR_FILENO, message, sizeof(message) - 1);
        _exit(127);
    }
    if (s.child < 0 || fcntl(s.fd, F_SETFD, FD_CLOEXEC) < 0 || fcntl(s.fd, F_SETFL, O_NONBLOCK) < 0) {
        s.error = std::string("Could not open a terminal: ") + std::strerror(errno);
        stop();
        return false;
    }
    return true;
}

void Session::stop() {
    auto &s = *impl_;
    if (s.child > 0) {
        // The foreground job may have a process group different from the shell.
        const auto foreground = s.fd >= 0 ? tcgetpgrp(s.fd) : -1;
        if (foreground > 0 && foreground != getpgrp()) kill(-foreground, SIGHUP);
        kill(-s.child, SIGHUP);
        kill(s.child, SIGKILL);
    }
    if (s.fd >= 0) close(s.fd);
    if (s.child > 0) {
        while (waitpid(s.child, nullptr, 0) < 0 && errno == EINTR) {}
    }
    s.fd = -1;
    s.child = -1;
    s.pending.clear();
}

bool Session::running() const { return impl_->fd >= 0; }

void Session::resize(cyui::TerminalSize size) {
    if (!running()) return;
    auto dimensions = windowSize(size);
    if (ioctl(impl_->fd, TIOCSWINSZ, &dimensions) < 0 && errno != EIO)
        impl_->error = std::string("Could not resize the terminal: ") + std::strerror(errno);
}

bool Session::write(std::string_view bytes) {
    if (!running() || bytes.size() > 1024*1024 - impl_->pending.size()) return false;
    impl_->pending.append(bytes);
    return true;
}

std::string Session::read() {
    if (!running()) return {};
    auto &s = *impl_;
    if (!s.pending.empty()) {
        const auto written = ::write(s.fd, s.pending.data(), s.pending.size());
        if (written > 0) s.pending.erase(0, std::size_t(written));
        else if (written < 0 && errno != EINTR && errno != EAGAIN && errno != EIO)
            s.error = std::string("Could not send terminal input: ") + std::strerror(errno);
    }
    char buffer[8192];
    std::string text;
    while (text.size() < 128*1024) {
        const auto count = ::read(s.fd, buffer, sizeof(buffer));
        if (count > 0) {
            text.append(buffer, std::size_t(count));
            continue;
        }
        if (count < 0 && errno == EINTR) continue;
        if (count < 0 && errno == EAGAIN) {
            if (waitpid(s.child, nullptr, WNOHANG) != s.child) break;
            s.child = -1;
            break;
        }
        if (count < 0 && errno != EIO)
            s.error = std::string("Could not read terminal output: ") + std::strerror(errno);
        stop();
        break;
    }
    if (s.child < 0 && running()) stop();
    return text;
}

const std::string &Session::error() const { return impl_->error; }

} // namespace terminal_example
