#include "cyui/modules/DocumentViews.h"
#include "DocumentLayout.h"

namespace cyui {
DocumentResult documentView(const char *id, DocumentState &s, const Document &doc, ImVec2 size) {
    return documentView(id, s, doc, size, {});
}
DocumentResult documentView(const char *id, DocumentState &s, const Document &doc, ImVec2 size,
                            const DocumentOptions &options) {
    ScopedID scope{id};
    ImGui::BeginChild("##document", size);
    if (s.revision != doc.revision()) {
        s.spoilers.clear();
        s.activeEmbeds.clear();
        s.selectionBegin = s.selectionEnd = 0;
        s.selecting = false;
        s.resizingImage = 0;
        s.revision = doc.revision();
    }
    DocumentRenderer renderer{s, doc, options};
    renderer.content();
    renderer.selection();
    if (!s.jumpTo.empty()) {
        auto it = s.anchors.find(s.jumpTo);
        if (it != s.anchors.end())
            ImGui::SetScrollY(it->second);
        s.jumpTo.clear();
    }
    s.scroll = ImGui::GetScrollY();
    s.scrollMax = ImGui::GetScrollMaxY();
    ImGui::EndChild();
    documentLightbox(s, options);
    return renderer.result;
}
TranscriptResult transcript(const char *id, TranscriptState &s, ArrayView<TranscriptMessage> messages,
                            ImVec2 size) {
    return transcript(id, s, messages, size, {});
}
TranscriptResult transcript(const char *id, TranscriptState &s, ArrayView<TranscriptMessage> messages,
                            ImVec2 size, const DocumentOptions &options) {
    ScopedID scope(id);
    TranscriptResult result;
    std::unordered_set<ItemId> present;
    ImGui::BeginChild("##transcript", size);
    const bool bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - dp(3);
    bool changed = false;
    const float width = ImGui::GetContentRegionAvail().x, spacing = ImGui::GetStyle().ItemSpacing.y;
    const ImVec4 layout{width, ImGui::GetFontSize(), ImGui::GetFrameHeight(), spacing};
    if (layout.x != s.layout.x || layout.y != s.layout.y || layout.z != s.layout.z ||
        layout.w != s.layout.w) {
        s.heights.clear(); // wrapping, fonts or spacing change every height
        s.layout = layout;
    }
    for (std::size_t n = 0; n < messages.count; ++n) {
        const auto &m = messages[n];
        present.insert(m.id);
        auto &doc = s.documents[m.id];
        const bool edited = doc.setMarkdown(m.content);
        changed = changed || edited;
        ItemScope key(m.id);
        // Offscreen messages measured before, with the same role/time, only reserve their height.
        const std::hash<std::string> hash;
        const std::size_t signature = hash(m.role) * 31 ^ hash(m.time);
        const auto known = s.heights.find(m.id);
        if (!edited && known != s.heights.end() && known->second.signature == signature &&
            !ImGui::IsRectVisible({std::max(1.f, width), known->second.height})) {
            ImGui::Dummy({1, std::max(0.f, known->second.height - spacing)});
            continue;
        }
        const float top = ImGui::GetCursorPosY();
        heading(m.role.c_str(), 3);
        ImGui::SameLine();
        textFaint(tr("%s").c_str(), m.time.c_str());
        if (ghostButton(trLabel("Copy message").c_str())) {
            ImGui::SetClipboardText(m.content.c_str());
            result.copy = m.id;
        }
        DocumentRenderer renderer{s.views[m.id], doc, options};
        renderer.content();
        renderer.selection();
        auto r = renderer.result;
        if (!r.link.empty())
            result.link = r.link;
        divider();
        s.heights[m.id] = {ImGui::GetCursorPosY() - top, signature};
    }
    if (s.follow && bottom && (changed || s.count != messages.count))
        ImGui::SetScrollHereY(1);
    s.count = messages.count;
    ImGui::EndChild();
    for (auto it = s.documents.begin(); it != s.documents.end();)
        if (!present.count(it->first)) {
            s.views.erase(it->first);
            s.heights.erase(it->first);
            it = s.documents.erase(it);
        } else
            ++it;
    return result;
}
} // namespace cyui
