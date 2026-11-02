#include "cyui/modules/Diagnostics.h"
#include "ModuleInternal.h"

namespace cyui {
void LogBuffer::append(LogLevel level, const std::string &text, const std::string &source) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t start = 0;
    do {
        const auto end = text.find('\n', start);
        std::string line = text.substr(start, end == std::string::npos ? end : end - start);
        if (line.size() > maxBytes_) {
            std::size_t endByte = maxBytes_;
            while (endByte && (static_cast<unsigned char>(line[endByte]) & 0xc0) == 0x80)
                --endByte;
            line.resize(endByte);
        }
        std::size_t sourceEnd = std::min(source.size(), maxBytes_ - line.size());
        while (sourceEnd < source.size() && sourceEnd &&
               (static_cast<unsigned char>(source[sourceEnd]) & 0xc0) == 0x80)
            --sourceEnd;
        std::string src = source.substr(0, sourceEnd);
        bytes_ += line.size() + src.size();
        rows_.push_back({next_++, level, std::move(line), std::move(src)});
        while (rows_.size() > capacity_ || bytes_ > maxBytes_) {
            bytes_ -= rows_.front().text.size() + rows_.front().source.size();
            rows_.pop_front();
        }
        if (end == std::string::npos)
            break;
        start = end + 1;
    } while (start < text.size());
    ++revision_;
}
void LogBuffer::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    rows_.clear();
    bytes_ = 0;
    ++revision_;
}
std::uint64_t LogBuffer::snapshot(std::vector<LogEntry> &out, std::uint64_t known) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (known != revision_)
        out.assign(rows_.begin(), rows_.end());
    return revision_;
}
namespace {
ImU32 logColor(LogLevel level) {
    const auto &p = palette();
    return level == LogLevel::error     ? p.error
           : level == LogLevel::warning ? p.warning
           : level == LogLevel::debug   ? p.textFaint
                                        : p.text;
}
} // namespace
ItemId logView(const char *id, LogState &s, const LogBuffer &buffer, ImVec2 size) {
    ScopedID scope(id);
    const auto previous = s.revision;
    s.revision = buffer.snapshot(s.rows, s.revision);
    ImGui::SetNextItemWidth(-dp(90));
    detail::inputText("##filter", s.query);
    ImGui::SameLine();
    ImGui::Checkbox("Follow", &s.follow);
    ItemId request = 0;
    ScopedFont font(monoFont());
    ImGui::BeginChild("##log", size, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    const bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - dp(3);
    std::vector<std::size_t> filtered;
    for (std::size_t i = 0; i < s.rows.size(); ++i)
        if ((s.levels & (1u << unsigned(s.rows[i].level))) &&
            detail::matches(s.rows[i].text + " " + s.rows[i].source, s.query))
            filtered.push_back(i);
    ImGuiListClipper clip;
    clip.Begin(int(filtered.size()));
    while (clip.Step())
        for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i) {
            const auto &row = s.rows[filtered[std::size_t(i)]];
            ItemScope key(row.id);
            ScopedColor color(ImGuiCol_Text, logColor(row.level));
            const auto pos = ImGui::GetCursorScreenPos();
            const float width =
                std::max(ImGui::GetContentRegionAvail().x, ImGui::CalcTextSize(row.text.c_str()).x);
            const bool clicked = ImGui::Selectable("##row", s.selected == row.id, 0,
                                                   {width, ImGui::GetTextLineHeightWithSpacing()});
            ImGui::GetWindowDrawList()->AddText(pos, logColor(row.level), row.text.c_str());
            if (clicked) {
                s.selected = row.id;
                request = row.id;
            }
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Copy"))
                    ImGui::SetClipboardText(row.text.c_str());
                ImGui::EndPopup();
            }
        }
    if (s.follow && atBottom && previous != s.revision)
        ImGui::SetScrollHereY(1);
    ImGui::EndChild();
    return request;
}
ItemId diagnosticList(const char *id, ArrayView<Diagnostic> rows, ImVec2 size) {
    ItemId request = 0;
    if (ImGui::BeginTable(id, 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                          size)) {
        ImGui::TableSetupColumn("Source");
        ImGui::TableSetupColumn("Position");
        ImGui::TableSetupColumn("Message");
        ImGui::TableHeadersRow();
        ImGuiListClipper clip;
        clip.Begin(int(rows.count));
        while (clip.Step())
            for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i) {
                const auto &row = rows[std::size_t(i)];
                ItemScope key(row.id);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                const auto pos = ImGui::GetCursorScreenPos();
                if (ImGui::Selectable("##source", false, ImGuiSelectableFlags_SpanAllColumns))
                    request = row.id;
                ImGui::GetWindowDrawList()->AddText(pos, palette().text, row.source.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%d:%d", row.line, row.column);
                ImGui::TableNextColumn();
                textColoured(logColor(row.level), "%s", row.message.c_str());
            }
        ImGui::EndTable();
    }
    return request;
}
} // namespace cyui
