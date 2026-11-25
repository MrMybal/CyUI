#include "cyui/modules/Diagnostics.h"
#include <climits>
#include "ModuleInternal.h"
#include <atomic>

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
std::uint64_t LogBuffer::firstRevision() {
    static std::atomic<std::uint64_t> instances{0};
    return ((instances.fetch_add(1) + 1) << 40) | 1;
}
void LogBuffer::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    rows_.clear();
    bytes_ = 0;
    ++revision_;
}
std::uint64_t LogBuffer::snapshot(std::vector<LogEntry> &out, std::uint64_t known) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (known == revision_)
        return revision_;
    if ((known >> 40) != (revision_ >> 40)) { // never read from this buffer
        out.assign(rows_.begin(), rows_.end());
        return revision_;
    }
    // Rows are immutable and their IDs consecutive, so a previous copy only loses rows at the front
    // and gains rows at the back: trim and append instead of copying everything again.
    std::size_t drop = 0;
    const ItemId first = rows_.empty() ? next_ : rows_.front().id;
    while (drop < out.size() && out[drop].id < first)
        ++drop;
    const std::size_t kept = out.size() - drop;
    const bool reuse = kept && kept <= rows_.size() && out[drop].id == first &&
                       out.back().id == rows_[kept - 1].id && out.back().text == rows_[kept - 1].text;
    if (reuse) {
        out.erase(out.begin(), out.begin() + std::ptrdiff_t(drop));
        out.insert(out.end(), rows_.begin() + std::ptrdiff_t(kept), rows_.end());
    } else
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
    ImGui::Checkbox(trLabel("Follow").c_str(), &s.follow);
    ItemId request = 0;
    ScopedFont font(monoFont());
    ImGui::BeginChild("##log", size, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    const bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - dp(3);
    // No query and every level shown: rows are listed directly. Otherwise the matching indices are
    // cached until the rows, the query or the level mask change.
    const bool everything = s.query.empty() && (s.levels & 15u) == 15u;
    if (!everything &&
        (s.filterRevision != s.revision || s.filterQuery != s.query || s.filterLevels != s.levels)) {
        const auto query = detail::folded(s.query);
        s.filtered.clear();
        for (std::size_t i = 0; i < s.rows.size(); ++i)
            if ((s.levels & (1u << unsigned(s.rows[i].level))) &&
                (query.empty() || detail::matchesPair(s.rows[i].text, s.rows[i].source, query)))
                s.filtered.push_back(i);
        s.filterRevision = s.revision;
        s.filterQuery = s.query;
        s.filterLevels = s.levels;
    }
    const std::size_t count = everything ? s.rows.size() : s.filtered.size();
    ImGuiListClipper clip;
    clip.Begin(int(count));
    while (clip.Step())
        for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i) {
            const auto &row = s.rows[everything ? std::size_t(i) : s.filtered[std::size_t(i)]];
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
                if (ImGui::MenuItem(trLabel("Copy").c_str()))
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
        ImGui::TableSetupColumn(trLabel("Source").c_str());
        ImGui::TableSetupColumn(trLabel("Position").c_str());
        ImGui::TableSetupColumn(trLabel("Message").c_str());
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
                ImGui::Text(tr("%d:%d").c_str(), row.line, row.column);
                ImGui::TableNextColumn();
                textColoured(logColor(row.level), tr("%s").c_str(), row.message.c_str());
            }
        ImGui::EndTable();
    }
    return request;
}

namespace {
const char *taskStatusLabel(TaskStatus status) {
    switch (status) {
    case TaskStatus::queued: return "Queued";
    case TaskStatus::running: return "Running";
    case TaskStatus::succeeded: return "Succeeded";
    case TaskStatus::warning: return "Warning";
    case TaskStatus::failed: return "Failed";
    case TaskStatus::canceled: return "Canceled";
    }
    return "Queued";
}

ImU32 taskColor(TaskStatus status) {
    const auto &p = palette();
    switch (status) {
    case TaskStatus::running: return p.info;
    case TaskStatus::succeeded: return p.success;
    case TaskStatus::warning: return p.warning;
    case TaskStatus::failed: return p.error;
    default: return p.textDim;
    }
}

float taskHeight() { return ImGui::GetTextLineHeightWithSpacing() * 2 + dp(12); }
} // namespace

bool taskRow(const char *id, const TaskRowData &row, bool selected, ImVec2 size) {
    ScopedID scope(id);
    ItemScope key(row.id);
    const auto pos = ImGui::GetCursorScreenPos();
    const float width = std::max(dp(1), size.x > 0 ? size.x : ImGui::GetContentRegionAvail().x);
    const float height = size.y > 0 ? size.y : taskHeight();
    const bool pressed = ImGui::Selectable("##task", selected, 0, {width, height});
    const auto &p = palette();
    auto *draw = ImGui::GetWindowDrawList();
    const float pad = dp(8), line = ImGui::GetTextLineHeightWithSpacing();
    const auto color = taskColor(row.status);
    const auto status = tr(taskStatusLabel(row.status));
    const auto statusWidth = ImGui::CalcTextSize(status.c_str()).x;
    const float titleX = pos.x + pad + dp(13);
    draw->PushClipRect(pos, {pos.x + width, pos.y + height}, true);
    draw->AddCircleFilled({pos.x + pad, pos.y + pad + ImGui::GetTextLineHeight() * .5f}, dp(3), color);
    clippedText(draw, {titleX, pos.y + pad}, std::max(0.f, width - (titleX - pos.x) - statusWidth - pad * 2),
                p.text, row.title.c_str());
    if (width > statusWidth + pad * 3) draw->AddText({pos.x + width - statusWidth - pad, pos.y + pad}, color, status.c_str());
    clippedText(draw, {titleX, pos.y + pad + line}, std::max(0.f, width - (titleX - pos.x) - pad),
                p.textDim, row.detail.c_str());
    if (row.status == TaskStatus::running || row.progress) {
        const ImVec2 lo{pos.x + pad, pos.y + height - dp(3)}, hi{pos.x + width - pad, pos.y + height - dp(1)};
        if (hi.x > lo.x) {
            draw->AddRectFilled(lo, hi, p.bg4);
            const float span = hi.x - lo.x;
            const bool known = row.progress && std::isfinite(*row.progress);
            const float start = known ? 0.f : float(std::fmod(ImGui::GetTime() * .65, 1.)) * .75f;
            const float end = known ? std::clamp(*row.progress, 0.f, 1.f) : start + .25f;
            if (end > start) draw->AddRectFilled({lo.x + span * start, lo.y}, {lo.x + span * end, hi.y}, color);
        }
    }
    draw->PopClipRect();
    return pressed;
}

TaskPanelResult taskPanel(const char *id, TaskPanelState &state, ArrayView<TaskRowData> rows, ImVec2 size) {
    ScopedID scope(id);
    const auto before = state.selected;
    const auto available = ImGui::GetContentRegionAvail();
    size = {size.x > 0 ? size.x : std::max(dp(1), available.x + size.x),
            size.y > 0 ? size.y : std::max(dp(1), available.y + size.y)};
    filterBox("##tasks-filter", state.filter);
    std::vector<std::size_t> filtered;
    const TaskRowData *active = nullptr;
    for (std::size_t n = 0; n < rows.count; ++n) {
        if (state.selected == rows[n].id) active = &rows[n];
        if (state.filter.matches(rows[n].title, rows[n].detail)) filtered.push_back(n);
    }
    if (!active) state.selected.reset();
    const float height = std::max(taskHeight() * 2, size.y * .46f);
    if (ImGui::BeginChild("##tasks", {size.x, height}, ImGuiChildFlags_Borders)) {
        if (filtered.empty()) emptyState(tr("No tasks to display").c_str(), "");
        ImGuiListClipper clip;
        clip.Begin(int(std::min<std::size_t>(filtered.size(), INT_MAX)), taskHeight() + ImGui::GetStyle().ItemSpacing.y);
        while (clip.Step()) {
            for (int index = clip.DisplayStart; index < clip.DisplayEnd; ++index) {
                const auto &row = rows[filtered[std::size_t(index)]];
                if (taskRow("##task-row", row, state.selected == row.id)) {
                    state.selected = row.id;
                    active = &row;
                }
            }
        }
    }
    ImGui::EndChild();
    TaskPanelResult result{state.selected, {}, before != state.selected};
    if (result.selectionChanged) state.log = {};
    if (active) {
        ItemScope selectedKey(active->id);
        sectionTitle(active->title.c_str());
        textColoured(taskColor(active->status), "%s", tr(taskStatusLabel(active->status)).c_str());
        if (active->canCancel) {
            ImGui::SameLine();
            if (dangerButton(trLabel("Cancel task").c_str())) result.cancel = active->id;
        }
        if (!active->detail.empty()) textDim("%s", active->detail.c_str());
        if (active->log) logView("##task-log", state.log, *active->log, {size.x, std::max(dp(70), size.y - height - dp(90))});
    }
    return result;
}
} // namespace cyui
