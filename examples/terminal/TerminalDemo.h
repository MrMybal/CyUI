#pragma once

#include "Session.h"

namespace terminal_example {

class TerminalDemo {
public:
    void pump();
    bool draw(); // False asks the gallery to display its static preview.

private:
    Session session_;
    cyui::Terminal terminal_;
    cyui::TerminalViewState view_;
    int shell_ = 0;
    bool hasSession_ = false, inputThrottled_ = false;
};

} // namespace terminal_example
