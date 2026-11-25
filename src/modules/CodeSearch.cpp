#include "cyui/modules/Code.h"
#include "ModuleInternal.h"
#include <climits>
#include <unordered_map>

namespace cyui {
namespace {
ImU32 markColor(Tone tone) {
    const auto &p = palette();
    switch (tone) {
    case Tone::warning: return p.warning;
    case Tone::error: return p.error;
    case Tone::success: return p.success;
    default: return p.accent;
    }
}

bool boundary(std::string_view text, std::size_t at) {
    return at <= text.size() && (at == text.size() || (static_cast<unsigned char>(text[at]) & 0xc0) != 0x80);
}

const TextMark *paintMarks(std::string_view text, std::size_t first, std::size_t last, ArrayView<TextMark> marks,
                           ImVec2 pos, float width, std::optional<ItemId> selected, bool hovered) {
    if (last <= first || width <= 0) return nullptr;
    const auto end = ImVec2{pos.x + width, pos.y + ImGui::GetTextLineHeight()};
    const auto mouse = ImGui::GetIO().MousePos;
    const auto *begin = text.data() + first;
    auto *draw = ImGui::GetWindowDrawList();
    const TextMark *hit = nullptr;
    draw->PushClipRect(pos, end, true);
    for (std::size_t i = 0; i < marks.count; ++i) {
        const auto &mark = marks[i];
        if (mark.end <= first || mark.begin >= last || mark.end <= mark.begin ||
            !boundary(text, mark.begin) || !boundary(text, mark.end)) continue;
        const auto lo = ImGui::CalcTextSize(begin, text.data() + std::max(mark.begin, first)).x;
        const auto hi = ImGui::CalcTextSize(begin, text.data() + std::min(mark.end, last)).x;
        const auto color = markColor(mark.tone);
        draw->AddRectFilled({pos.x + lo, pos.y}, {pos.x + hi, end.y}, alpha(color, selected == mark.id ? .38f : .21f));
        if (selected == mark.id) draw->AddLine({pos.x + lo, end.y - dp(1)}, {pos.x + hi, end.y - dp(1)}, color);
        if (hovered && mouse.x >= pos.x + lo && mouse.x <= std::min(end.x, pos.x + hi) &&
            mouse.y >= pos.y && mouse.y <= end.y && (!hit || mark.end - mark.begin < hit->end - hit->begin)) hit = &mark;
    }
    draw->AddText(pos, palette().text, begin, text.data() + last);
    draw->PopClipRect();
    return hit;
}

std::size_t previewEnd(std::string_view excerpt) {
    const auto end = excerpt.find_first_of("\r\n");
    return end == excerpt.npos ? excerpt.size() : end;
}
} // namespace

std::optional<ItemId> annotatedText(const char *id, AnnotatedTextState &state, std::string_view text,
                                    ArrayView<TextMark> marks, ImVec2 size, std::uint64_t revision) {
    ScopedID scope(id);
    if (!revision || state.revision != revision || state.length != text.size() || state.lineOffsets.empty()) {
        state.lineOffsets.assign(1, 0);
        for (std::size_t at = text.find('\n'); at != text.npos; at = text.find('\n', at + 1))
            state.lineOffsets.push_back(at + 1);
        state.revision = revision;
        state.length = text.size();
    }
    const TextMark *current = nullptr;
    for (std::size_t i = 0; state.selected && i < marks.count; ++i) {
        if (marks[i].id == state.selected && marks[i].begin < marks[i].end &&
            boundary(text, marks[i].begin) && boundary(text, marks[i].end)) { current = &marks[i]; break; }
    }
    if (!current) state.selected.reset();
    std::optional<ItemId> action;
    ScopedFont font(monoFont());
    if (ImGui::BeginChild("##text", size, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGuiListClipper clip;
        clip.Begin(int(std::min<std::size_t>(state.lineOffsets.size(), INT_MAX)), ImGui::GetTextLineHeightWithSpacing());
        while (clip.Step()) for (auto index = clip.DisplayStart; index < clip.DisplayEnd; ++index) {
            const auto row = std::size_t(index), begin = state.lineOffsets[row];
            auto end = row + 1 < state.lineOffsets.size() ? state.lineOffsets[row + 1] - 1 : text.size();
            if (end > begin && text[end - 1] == '\r') --end;
            const float textWidth = end > begin ? ImGui::CalcTextSize(text.data() + begin, text.data() + end).x : 0;
            const float width = std::max({dp(1), textWidth, ImGui::GetContentRegionAvail().x});
            const auto pos = ImGui::GetCursorScreenPos();
            ItemScope line(row);
            const bool pressed = ImGui::Selectable("##text-line", false, 0, {width, ImGui::GetTextLineHeight()});
            const auto *hit = paintMarks(text, begin, end, marks, pos, width, state.selected, ImGui::IsItemHovered());
            if (hit && !hit->label.empty()) ImGui::SetTooltip("%s", hit->label.c_str());
            if (pressed && hit) { state.selected = action = hit->id; current = hit; }
            if (ImGui::BeginPopupContextItem()) {
                const auto *copy = current;
                if (copy && ImGui::MenuItem(trLabel("Copy marked text").c_str()))
                    ImGui::SetClipboardText(std::string(text.substr(copy->begin, copy->end - copy->begin)).c_str());
                ImGui::EndPopup();
            }
        }
    }
    ImGui::EndChild();
    return action;
}

SearchResultsResult searchResults(const char *id, SearchResultsState &state, ArrayView<SearchHit> hits,
                                 ImVec2 size, const SearchResultsOptions &options) {
    ScopedID scope(id);
    filterBox("##filter-results", state.filter);
    const auto available = ImGui::GetContentRegionAvail();
    const float width = size.x > 0 ? size.x : std::max(dp(1), available.x + size.x);
    const float height = size.y > 0 ? size.y : std::max(dp(1), available.y + size.y);
    struct Group { std::string_view name; std::vector<const SearchHit *> hits; };
    std::vector<Group> groups;
    std::unordered_map<std::string_view, std::size_t> bySource;
    const SearchHit *active = nullptr;
    for (std::size_t n = 0; n < hits.count; ++n) {
        const auto &hit = hits[n];
        if (state.selected == hit.id) active = &hit;
        if (!state.filter.matches(hit.source, hit.excerpt)) continue;
        auto [entry, inserted] = bySource.try_emplace(hit.source, groups.size());
        if (inserted) groups.push_back({hit.source, {}});
        groups[entry->second].hits.push_back(&hit);
    }
    if (!active) state.selected.reset();
    SearchResultsResult result;
    if (ImGui::BeginChild("##hits", {width, std::max(dp(70), height * .53f)}, ImGuiChildFlags_Borders)) {
        if (groups.empty()) emptyState(tr("No results").c_str(), "");
        // Stable grouping preserves the source and hit order supplied by the host.
        for (const auto &group : groups) {
            ScopedID key(group.name.empty() ? "##empty-source" : group.name.data());
            const std::string name = group.name.empty() ? tr("Untitled") : std::string(group.name);
            if (!ImGui::TreeNodeEx("##source", ImGuiTreeNodeFlags_DefaultOpen, "%s (%zu)", name.c_str(), group.hits.size())) continue;
            const float lineHeight = ImGui::GetTextLineHeight();
            ImGuiListClipper clip;
            clip.Begin(int(std::min<std::size_t>(group.hits.size(), INT_MAX)), ImGui::GetTextLineHeightWithSpacing());
            while (clip.Step()) for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i) {
                const auto &hit = *group.hits[std::size_t(i)];
                ItemScope hitKey(hit.id);
                const auto pos = ImGui::GetCursorScreenPos();
                const float room = std::max(dp(1), ImGui::GetContentRegionAvail().x);
                const bool activated = ImGui::Selectable("##hit", state.selected == hit.id, ImGuiSelectableFlags_AllowDoubleClick, {room, lineHeight});
                if (activated) {
                    active = &hit;
                    state.selected = hit.id;
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) result.reveal = hit.id;
                }
                const auto location = hit.line ? std::to_string(hit.line) + ":" + std::to_string(hit.column) : std::string();
                const float prefix = location.empty() ? 0 : ImGui::CalcTextSize(location.c_str()).x + dp(10);
                auto *draw = ImGui::GetWindowDrawList();
                draw->AddText(pos, palette().textDim, location.c_str());
                paintMarks(hit.excerpt, 0, previewEnd(hit.excerpt), hit.highlights, {pos.x + prefix, pos.y},
                           room - prefix, {}, false);
            }
            ImGui::TreePop();
        }
    }
    ImGui::EndChild();
    if (active) {
        if (button(trLabel("Reveal result").c_str())) result.reveal = active->id;
        if (auto *preview = options.preview ? options.preview(*active) : nullptr) {
            if (state.previewed != active->id) {
                state.preview = {};
                state.preview.searchBar = false;
                if (active->line) state.preview.focusLine = int(std::min<std::size_t>(active->line - 1, INT_MAX));
            }
            state.previewed = active->id;
            const auto view = codeViewer("##preview", state.preview, *preview, {width, std::max(dp(85), height * .4f)});
            if (view.clickedLine >= 0) result.reveal = active->id;
        } else {
            state.previewed.reset();
            ImGui::TextWrapped("%s", active->excerpt.c_str());
        }
    }
    result.selected = state.selected;
    return result;
}

} // namespace cyui
