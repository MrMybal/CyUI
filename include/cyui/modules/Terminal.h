#pragma once

#include "cyui/Core.h"
#include <memory>
#include <string>
#include <string_view>

namespace cyui {

struct TerminalSize {
    int columns = 80;
    int rows = 24;
};

struct TerminalViewState;
struct TerminalOptions;

// An in-memory VT session. Call on the UI thread. The host owns process I/O.
class CYUI_API Terminal {
public:
    explicit Terminal(TerminalSize size = {}, unsigned scrollbackLines = 5000);
    ~Terminal();
    Terminal(const Terminal &) = delete;
    Terminal &operator=(const Terminal &) = delete;

    void reset();
    bool resize(TerminalSize size);
    TerminalSize size() const;
    void feed(std::string_view bytes);           // raw UTF-8 and VT bytes from the shell
    std::string takeInput();                    // key, paste, mouse and protocol reply bytes
    std::string text() const;                   // scrollback and current screen, as plain UTF-8
    void paste(std::string_view utf8);          // honours bracketed-paste mode

private:
    struct Model;
    std::unique_ptr<Model> model_;
    friend bool terminalView(const char *, Terminal &, TerminalViewState &, ImVec2, const TerminalOptions &);
};

struct TerminalViewState {
    // Widget state. Store one instance next to each Terminal.
    long long topRow = 0;
    long long anchorRow = -1, selectionRow = -1;
    int anchorColumn = 0, selectionColumn = 0;
    unsigned revision = 0;
    bool follow = true, focused = false, selecting = false, reportingMouse = false;
};

struct TerminalOptions {
    bool interactive = true;
    bool focus = false;                         // request keyboard focus this frame
};

// Fills the available space by default. Returns true when the grid size changed.
CYUI_API bool terminalView(const char *id, Terminal &, TerminalViewState &, ImVec2 size = {},
                           const TerminalOptions &options = {});

} // namespace cyui
