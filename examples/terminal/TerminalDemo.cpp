#include "TerminalDemo.h"
#include <cyui/CoreWidgets.h>
#include <algorithm>

namespace terminal_example {

void TerminalDemo::pump() {
    if (!session_.running()) return;
    terminal_.feed(session_.read());
    auto bytes = terminal_.takeInput();
    if (!bytes.empty() && session_.running()) inputThrottled_ = !session_.write(bytes);
}

bool TerminalDemo::draw() {
    using namespace cyui;
    const auto source = Session::shells();
    const auto first = tr(source[0]), second = tr(source[1]);
    const char *const labels[]{first.c_str(), second.c_str()};
    ImGui::BeginDisabled(session_.running());
    combo("##shell-profile", &shell_, labels, 2, dp(164));
    ImGui::EndDisabled();
    ImGui::SameLine();
    bool start = false;
    if (session_.running()) {
        if (button(trLabel("Stop").c_str())) session_.stop();
    } else {
        start = primaryButton(trLabel("Start session").c_str());
    }
    ImGui::SameLine();
    if (session_.running()) {
        const auto &p = palette();
        ImGui::TextColored(vec(p.success), "%s", tr("Connected").c_str());
    } else {
        textDim("%s", tr(hasSession_ ? "Session ended" : "Preview").c_str());
    }
    if (!session_.error().empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, palette().error);
        ImGui::TextWrapped("%s", session_.error().c_str());
        ImGui::PopStyleColor();
    }
    if (inputThrottled_) textDim("%s", tr("Some input was dropped because the shell was busy.").c_str());
    if (start) {
        terminal_.reset();
        inputThrottled_ = false;
    }
    if (!hasSession_ && !start) return false;

    const auto footer = tr("Ctrl+Shift+C / V to copy and paste. Shift+Page Up / Down to browse output.");
    const float footerHeight = ImGui::CalcTextSize(footer.c_str(), nullptr, false, ImGui::GetContentRegionAvail().x).y;
    TerminalOptions options;
    options.interactive = session_.running() || start;
    options.focus = start;
    const float height = std::max(dp(210), ImGui::GetContentRegionAvail().y - footerHeight - ImGui::GetStyle().ItemSpacing.y);
    const bool resized = terminalView("##live-terminal", terminal_, view_, {0, height}, options);
    ImGui::PushStyleColor(ImGuiCol_Text, palette().textDim);
    ImGui::TextWrapped("%s", footer.c_str());
    ImGui::PopStyleColor();
    if (start) hasSession_ = session_.start(shell_, terminal_.size()) || hasSession_;
    else if (resized) session_.resize(terminal_.size());
    return true;
}

} // namespace terminal_example
