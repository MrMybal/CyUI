#include "cyui/modules/DocumentViews.h"
#include <sstream>
#include <unordered_set>
#include <regex>
#include <stdexcept>

namespace cyui {
namespace {
std::vector<std::string> cells(std::string line) {
    if (!line.empty() && line.front() == '|')
        line.erase(0, 1);
    if (!line.empty() && line.back() == '|')
        line.pop_back();
    std::vector<std::string> out;
    std::istringstream stream(line);
    std::string cell;
    while (std::getline(stream, cell, '|')) {
        const auto a = cell.find_first_not_of(" \t"), b = cell.find_last_not_of(" \t");
        out.push_back(a == std::string::npos ? "" : cell.substr(a, b - a + 1));
    }
    return out;
}
CodeLanguage languageOf(const std::string &name) {
    return name == "python"                       ? CodeLanguage::python
           : name == "json"                       ? CodeLanguage::json
           : name == "js" || name == "javascript" ? CodeLanguage::javascript
           : name == "ts" || name == "typescript" ? CodeLanguage::typescript
                                                  : CodeLanguage::cpp;
}
} // namespace
bool Document::setMarkdown(const std::string &text) {
    if (text == text_ && !blocks_.empty())
        return false;
    if (text.size() > 32 * 1024 * 1024)
        throw std::length_error("Document exceeds 32 MiB");
    text_ = text;
    blocks_.clear();
    std::istringstream stream(text);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        lines.push_back(line);
    }
    const std::regex list(R"(^\s*(?:[-*+] |\d+[.)] )(.*)$)"), headingPattern(R"(^(#{1,6})\s+(.*)$)"),
        separator(R"(^[\s|:\-]+$)");
    for (std::size_t at = 0; at < lines.size();) {
        line = lines[at++];
        DocumentBlock block;
        block.text = line;
        std::smatch match;
        if (line.rfind("```", 0) == 0 || line.rfind("~~~", 0) == 0) {
            block.kind = DocumentBlockKind::code;
            block.language = line.substr(3);
            block.text.clear();
            const auto fence = line.substr(0, 3);
            while (at < lines.size() && lines[at].rfind(fence, 0) != 0)
                block.text += lines[at++] + '\n';
            if (at < lines.size())
                ++at;
            block.code.setText(block.text, languageOf(block.language));
        } else if (line.empty())
            continue;
        else if (at < lines.size() && line.find('|') != std::string::npos &&
                 lines[at].find('-') != std::string::npos && std::regex_match(lines[at], separator)) {
            block.kind = DocumentBlockKind::table;
            block.rows.push_back(cells(line));
            ++at;
            while (at < lines.size() && lines[at].find('|') != std::string::npos)
                block.rows.push_back(cells(lines[at++]));
        } else if (line == "---" || line == "***" || line == "___")
            block.kind = DocumentBlockKind::rule;
        else if (std::regex_match(line, match, headingPattern)) {
            block.kind = DocumentBlockKind::heading;
            block.level = int(match[1].length());
            block.text = match[2];
        } else if (line.rfind("> ", 0) == 0) {
            block.kind = DocumentBlockKind::quote;
            block.text = line.substr(2);
        } else if (std::regex_match(line, match, list)) {
            block.kind = DocumentBlockKind::list;
            block.text = match[1];
        }
        blocks_.push_back(std::move(block));
    }
    return true;
}
namespace {
void inlineText(const std::string &text, DocumentResult &result) {
    static const std::regex tokens(R"(\[[^\]]+\]\([^)]+\)|`[^`]+`|\*\*[^*]+\*\*|\*[^*]+\*|[^\s]+\s*)"),
        link(R"(^\[([^\]]+)\]\(([^)]+)\)$)");
    const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    bool first = true;
    std::size_t previousEnd = 0;
    for (std::sregex_iterator it(text.begin(), text.end(), tokens), end; it != end; ++it) {
        ScopedID tokenId{int(it->position())};
        const float gap = std::size_t(it->position()) > previousEnd ? ImGui::CalcTextSize(" ").x : 0;
        previousEnd = std::size_t(it->position() + it->length());
        const std::string token = it->str();
        std::smatch match;
        const bool linked = std::regex_match(token, match, link), mono = token.front() == '`',
                   strong = token.rfind("**", 0) == 0, emphasis = !strong && token.front() == '*';
        const auto label = linked             ? match[1].str()
                           : mono || emphasis ? token.substr(1, token.size() - 2)
                           : strong           ? token.substr(2, token.size() - 4)
                                              : token;
        ImFont *font = mono ? monoFont() : strong ? boldFont() : nullptr;
        if (font)
            ImGui::PushFont(font, 0);
        const float width = ImGui::CalcTextSize(label.c_str()).x;
        if (!first && ImGui::GetItemRectMax().x + width + gap < right)
            ImGui::SameLine(0, gap);
        if (linked) {
            if (linkText(label.c_str()))
                result.link = match[2];
        } else if (mono)
            textColoured(palette().accent, "%s", label.c_str());
        else if (emphasis)
            textDim("%s", label.c_str());
        else
            ImGui::TextWrapped("%s", label.c_str());
        if (font)
            ImGui::PopFont();
        first = false;
    }
}
DocumentResult content(DocumentState &s, const Document &doc) {
    DocumentResult result;
    s.codeViews.resize(doc.blocks().size());
    for (std::size_t n = 0; n < doc.blocks().size(); ++n) {
        const auto &b = doc.blocks()[n];
        ScopedID key{int(n)};
        switch (b.kind) {
        case DocumentBlockKind::heading:
            heading(b.text.c_str(), std::min(3, b.level));
            break;
        case DocumentBlockKind::rule:
            divider();
            break;
        case DocumentBlockKind::quote:
            callout(Tone::info, b.text.c_str());
            break;
        case DocumentBlockKind::list:
            ImGui::Bullet();
            ImGui::SameLine();
            inlineText(b.text, result);
            break;
        case DocumentBlockKind::code: {
            textFaint("%s", b.language.c_str());
            ImGui::SameLine();
            if (ghostButton("Copy")) {
                ImGui::SetClipboardText(b.text.c_str());
                result.copiedBlock = int(n);
            }
            codeViewer("##block", s.codeViews[n], b.code,
                       {0, dp(std::clamp(float(b.code.lines().size()) * 20 + 48, 90.f, 320.f))});
            break;
        }
        case DocumentBlockKind::table: {
            std::size_t columns = 0;
            for (const auto &row : b.rows)
                columns = std::max(columns, row.size());
            columns = std::min<std::size_t>(32, columns);
            if (columns && ImGui::BeginTable("##table", int(columns),
                                             ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                                 ImGuiTableFlags_SizingStretchSame)) {
                for (std::size_t r = 0; r < b.rows.size(); ++r) {
                    ScopedID rowId{int(r)};
                    ImGui::TableNextRow(r == 0 ? ImGuiTableRowFlags_Headers : 0);
                    for (std::size_t col = 0; col < std::min(columns, b.rows[r].size()); ++col) {
                        ScopedID cellId{int(col)};
                        ImGui::TableNextColumn();
                        inlineText(b.rows[r][col], result);
                    }
                }
                ImGui::EndTable();
            }
            break;
        }
        default:
            inlineText(b.text, result);
            break;
        }
        space(3);
    }
    return result;
}
} // namespace
DocumentResult documentView(const char *id, DocumentState &s, const Document &doc, ImVec2 size) {
    ScopedID scope(id);
    ImGui::BeginChild("##document", size);
    auto result = content(s, doc);
    ImGui::EndChild();
    return result;
}
TranscriptResult transcript(const char *id, TranscriptState &s, ArrayView<TranscriptMessage> messages,
                            ImVec2 size) {
    ScopedID scope(id);
    TranscriptResult result;
    std::unordered_set<ItemId> present;
    ImGui::BeginChild("##transcript", size);
    const bool bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - dp(3);
    bool changed = false;
    for (std::size_t n = 0; n < messages.count; ++n) {
        const auto &m = messages[n];
        present.insert(m.id);
        auto &doc = s.documents[m.id];
        changed = doc.setMarkdown(m.content) || changed;
        ItemScope key(m.id);
        heading(m.role.c_str(), 3);
        ImGui::SameLine();
        textFaint("%s", m.time.c_str());
        if (ghostButton("Copy message")) {
            ImGui::SetClipboardText(m.content.c_str());
            result.copy = m.id;
        }
        auto r = content(s.views[m.id], doc);
        if (!r.link.empty())
            result.link = r.link;
        divider();
    }
    if (s.follow && bottom && (changed || s.count != messages.count))
        ImGui::SetScrollHereY(1);
    s.count = messages.count;
    ImGui::EndChild();
    for (auto it = s.documents.begin(); it != s.documents.end();)
        if (!present.count(it->first)) {
            s.views.erase(it->first);
            it = s.documents.erase(it);
        } else
            ++it;
    return result;
}
} // namespace cyui
