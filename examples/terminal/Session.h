#pragma once

#include <cyui/modules/Terminal.h>
#include <memory>
#include <string>
#include <string_view>

namespace terminal_example {

// A local interactive child process. All public calls are made on the host thread.
class Session {
public:
    Session();
    ~Session();
    static const char *const *shells(); // two platform-specific choices

    bool start(int shell, cyui::TerminalSize size);
    void stop();
    bool running() const;
    void resize(cyui::TerminalSize size);
    bool write(std::string_view bytes);
    std::string read();
    const std::string &error() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace terminal_example
