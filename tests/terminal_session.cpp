#include "Session.h"
#include <chrono>
#include <cstdio>
#include <thread>

int main() {
    using namespace std::chrono_literals;
    terminal_example::Session session;
    for (int profile : {1, 0}) {
        if (!session.start(profile, {80, 24})) {
            std::fprintf(stderr, "%s\n", session.error().c_str());
            return 1;
        }
        cyui::Terminal terminal({111, 61});
        session.resize(terminal.size());
#ifdef _WIN32
        const auto startupDeadline = std::chrono::steady_clock::now() + 8s;
        while (session.running() && std::chrono::steady_clock::now() < startupDeadline) {
            terminal.feed(session.read());
            if (auto input = terminal.takeInput(); !input.empty()) session.write(input);
            const auto text = terminal.text();
            if (!text.empty() && text.back() == '>') break;
            std::this_thread::sleep_for(10ms);
        }
        const auto *command = profile == 0 ? "Write-Output ('cyui_terminal' + '_live_OK')\r" :
            "set CYUI_TERMINAL_TEST=cyui_terminal\recho %CYUI_TERMINAL_TEST%_live_OK\r";
#else
        const auto *command = "printf 'cyui_terminal_live_%s\\n' OK; stty size\r";
#endif
        if (!session.write(command)) {
            std::fprintf(stderr, "%s was not ready. %s\n%s\n", session.shells()[profile],
                         session.error().c_str(), terminal.text().c_str());
            return 1;
        }
        std::string display;
        bool verified = false;
        const auto deadline = std::chrono::steady_clock::now() + 12s;
        while (session.running() && std::chrono::steady_clock::now() < deadline) {
            if (auto data = session.read(); !data.empty()) {
                terminal.feed(data);
                if (auto input = terminal.takeInput(); !input.empty()) session.write(input);
                display = terminal.text();
                if (display.find("cyui_terminal_live_OK") != std::string::npos
#ifndef _WIN32
                    && display.find("61 111") != std::string::npos
#endif
                ) {
                    verified = true;
                    break;
                }
            }
            std::this_thread::sleep_for(10ms);
        }
        if (!verified) {
            std::fprintf(stderr, "%s command or resize failed. %s\n%s\n",
                         session.shells()[profile], session.error().c_str(), display.c_str());
            return 1;
        }
        if (!session.write("exit\r")) return 1;
        const auto exitDeadline = std::chrono::steady_clock::now() + 5s;
        while (session.running() && std::chrono::steady_clock::now() < exitDeadline) {
            session.read();
            std::this_thread::sleep_for(10ms);
        }
        if (session.running()) {
            std::fprintf(stderr, "%s did not exit\n", session.shells()[profile]);
            return 1;
        }
    }
    return session.start(1, {80, 24}) ? (session.stop(), session.running() ? 1 : 0) : 1;
}
