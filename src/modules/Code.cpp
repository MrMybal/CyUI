#include "cyui/modules/Code.h"
#include "ModuleInternal.h"
#include <set>
#include <cstdio>
#include <stdexcept>

namespace cyui {
std::size_t displayOffset(std::string_view text, std::size_t end, unsigned tabs) {
    tabs = std::max(1u, tabs);
    end = std::min(end, text.size());
    std::size_t bytes = 0, column = 0;
    for (std::size_t i = 0; i < end; ++i) {
        const auto c = static_cast<unsigned char>(text[i]);
        if (c == '\t') {
            const auto n = tabs - column % tabs;
            bytes += n;
            column += n;
        } else {
            ++bytes;
            if ((c & 0xc0) != 0x80)
                ++column;
        }
    }
    return bytes;
}
std::string expandTabs(std::string_view text, unsigned tabs) {
    tabs = std::max(1u, tabs);
    std::string out;
    std::size_t column = 0;
    for (unsigned char c : text) {
        if (c == '\t') {
            const auto n = tabs - column % tabs;
            out.append(n, ' ');
            column += n;
        } else {
            out += char(c);
            if ((c & 0xc0) != 0x80)
                ++column;
        }
    }
    return out;
}
namespace {
bool identifier(unsigned char c) {
    return c >= 128 || std::isalnum(c) || c == '_';
}
bool keyword(std::string_view s) {
    static const std::set<std::string_view> words = {
        "alignas",   "async",    "await",    "break",   "case",      "catch",  "class",   "const",
        "constexpr", "continue", "def",      "default", "delete",    "do",     "else",    "enum",
        "export",    "extern",   "false",    "finally", "fn",        "for",    "from",    "function",
        "if",        "impl",     "import",   "in",      "interface", "let",    "match",   "namespace",
        "new",       "None",     "null",     "nullptr", "override",  "pass",   "private", "protected",
        "pub",       "public",   "raise",    "return",  "self",      "sizeof", "static",  "struct",
        "super",     "switch",   "template", "this",    "throw",     "trait",  "true",    "try",
        "type",      "typedef",  "using",    "var",     "virtual",   "void",   "while",   "with",
        "yield"};
    return words.count(s) != 0;
}
bool typeWord(std::string_view s) {
    static const std::set<std::string_view> words = {
        "auto",     "bool",     "byte", "char",   "double",  "float",   "float2",   "float3",
        "float4",   "float4x4", "half", "int",    "int32_t", "int64_t", "long",     "short",
        "signed",   "size_t",   "std",  "string", "String",  "uint",    "uint32_t", "uint64_t",
        "unsigned", "Vec2",     "Vec3", "vec2",   "vec3",    "vec4"};
    return words.count(s) != 0;
}
std::vector<CodeSpan> tokenize(const std::string &text, CodeLanguage language, bool &comment,
                               std::string &quote) {
    std::vector<CodeSpan> spans;
    if (language == CodeLanguage::plain) {
        spans.push_back({0, text.size(), TokenRole::text});
        return spans;
    }
    const bool hash = language == CodeLanguage::python || language == CodeLanguage::ini;
    const bool dash = language == CodeLanguage::sql || language == CodeLanguage::lua;
    std::size_t i = 0;
    auto add = [&](std::size_t a, std::size_t b, TokenRole role) {
        if (b > a)
            spans.push_back({a, b - a, role});
    };
    while (i < text.size()) {
        const auto start = i;
        if (comment) {
            const auto at = text.find("*/", i);
            i = at == std::string::npos ? text.size() : at + 2;
            comment = at == std::string::npos;
            add(start, i, TokenRole::comment);
            continue;
        }
        if (!quote.empty()) {
            const auto at = text.find(quote, i);
            i = at == std::string::npos ? text.size() : at + quote.size();
            if (at != std::string::npos)
                quote.clear();
            add(start, i, TokenRole::string);
            continue;
        }
        if ((hash && text[i] == '#') || (dash && text.compare(i, 2, "--") == 0) ||
            (language == CodeLanguage::ini && text[i] == ';') ||
            (!hash && !dash && text.compare(i, 2, "//") == 0)) {
            add(i, text.size(), TokenRole::comment);
            break;
        }
        if (!hash && text.compare(i, 2, "/*") == 0) {
            comment = true;
            i += 2;
            const auto at = text.find("*/", i);
            i = at == std::string::npos ? text.size() : at + 2;
            comment = at == std::string::npos;
            add(start, i, TokenRole::comment);
            continue;
        }
        if (text[i] == '#' && !hash) {
            add(i, text.size(), TokenRole::preprocessor);
            break;
        }
        if (text[i] == '\'' || text[i] == '"' || text[i] == '`') {
            const char q = text[i++];
            if (language == CodeLanguage::python && text.compare(start, 3, std::string(3, q)) == 0) {
                quote = std::string(3, q);
                i = start + 3;
                const auto at = text.find(quote, i);
                i = at == std::string::npos ? text.size() : at + 3;
                if (at != std::string::npos)
                    quote.clear();
            } else {
                while (i < text.size()) {
                    const char c = text[i++];
                    if (c == '\\' && i < text.size())
                        ++i;
                    else if (c == q)
                        break;
                }
            }
            add(start, i, TokenRole::string);
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(text[i]))) {
            ++i;
            while (i < text.size() && (identifier(static_cast<unsigned char>(text[i])) || text[i] == '.'))
                ++i;
            add(start, i, TokenRole::number);
            continue;
        }
        if (identifier(static_cast<unsigned char>(text[i]))) {
            ++i;
            while (i < text.size() && identifier(static_cast<unsigned char>(text[i])))
                ++i;
            const std::string_view word(text.data() + start, i - start);
            std::size_t next = i;
            while (next < text.size() && std::isspace(static_cast<unsigned char>(text[next])))
                ++next;
            add(start, i,
                keyword(word)                             ? TokenRole::keyword
                : typeWord(word)                          ? TokenRole::type
                : next < text.size() && text[next] == '(' ? TokenRole::function
                                                          : TokenRole::text);
            continue;
        }
        ++i;
        add(start, i, TokenRole::text);
    }
    return spans;
}
double luminance(ImU32 color) {
    const auto v = vec(color);
    auto l = [](float c) { return c <= .04045f ? c / 12.92 : std::pow((c + .055) / 1.055, 2.4); };
    return .2126 * l(v.x) + .7152 * l(v.y) + .0722 * l(v.z);
}
} // namespace
bool CodeDocument::setText(const std::string &text, CodeLanguage language, std::uint64_t revision) {
    if (text.size() > 32 * 1024 * 1024)
        throw std::length_error("CodeDocument exceeds 32 MiB; supply a paged document");
    if (text_ == text && language_ == language && !lines_.empty()) {
        if (revision && revision != revision_) {
            revision_ = revision;
            for (auto &l : lines_)
                l.overlays.clear();
        }
        return false;
    }
    text_ = text;
    language_ = language;
    revision_ = revision ? revision : revision_ + 1;
    columns_ = 0;
    lines_.clear();
    bool comment = false;
    std::string quote;
    std::size_t start = 0;
    do {
        const auto end = text.find('\n', start);
        std::string raw = text.substr(start, end == std::string::npos ? end : end - start);
        if (!raw.empty() && raw.back() == '\r')
            raw.pop_back();
        auto display = expandTabs(raw);
        columns_ = std::max(columns_, display.size());
        auto spans = tokenize(display, language, comment, quote);
        lines_.push_back({std::move(display), std::move(spans), {}});
        if (end == std::string::npos)
            break;
        start = end + 1;
    } while (start <= text.size());
    return true;
}
bool CodeDocument::setOverlays(ArrayView<SourceSpan> spans, std::uint64_t revision) {
    if (revision != revision_)
        return false;
    for (auto &l : lines_)
        l.overlays.clear();
    std::vector<std::string_view> raw;
    std::size_t start = 0;
    do {
        const auto end = text_.find('\n', start);
        raw.emplace_back(text_.data() + start, (end == std::string::npos ? text_.size() : end) - start);
        if (end == std::string::npos)
            break;
        start = end + 1;
    } while (start <= text_.size());
    for (std::size_t i = 0; i < spans.count; ++i) {
        const auto &s = spans[i];
        if (s.line >= lines_.size() || s.endByte < s.startByte || s.endByte > raw[s.line].size())
            continue;
        const auto boundary = [&](std::size_t at) {
            return at == raw[s.line].size() || (static_cast<unsigned char>(raw[s.line][at]) & 0xc0) != 0x80;
        };
        if (!boundary(s.startByte) || !boundary(s.endByte))
            continue;
        const auto a = displayOffset(raw[s.line], s.startByte), b = displayOffset(raw[s.line], s.endByte);
        lines_[s.line].overlays.push_back({a, b - a, s.role});
    }
    return true;
}
ImU32 tokenColor(TokenRole role, ImU32 background) {
    const auto &p = palette();
    ImU32 c = p.text;
    switch (role) {
    case TokenRole::keyword:
        c = p.accent;
        break;
    case TokenRole::type:
        c = p.info;
        break;
    case TokenRole::number:
        c = p.warning;
        break;
    case TokenRole::string:
        c = p.success;
        break;
    case TokenRole::comment:
        c = p.textFaint;
        break;
    case TokenRole::preprocessor:
        c = mix(p.info, p.accent, .5f);
        break;
    case TokenRole::function:
        c = mix(p.text, p.warning, .4f);
        break;
    default:
        break;
    }
    if (!background)
        background = p.bg0;
    const double ground = luminance(background);
    for (int i = 0; i <= 10; ++i) {
        const auto candidate = mix(c, p.text, i / 10.f);
        const double light = luminance(candidate);
        if ((std::max(ground, light) + .05) / (std::min(ground, light) + .05) >= 4.5)
            return candidate;
    }
    return p.text;
}
void paintCodeLine(ImDrawList *d, ImVec2 p, const CodeLine &line, ImU32 background) {
    std::vector<std::size_t> cuts{0, line.text.size()};
    for (const auto &span : line.spans) {
        cuts.push_back(std::min(span.start, line.text.size()));
        cuts.push_back(std::min(span.start + span.length, line.text.size()));
    }
    for (const auto &span : line.overlays) {
        cuts.push_back(std::min(span.start, line.text.size()));
        cuts.push_back(std::min(span.start + span.length, line.text.size()));
    }
    std::sort(cuts.begin(), cuts.end());
    cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
    std::size_t lexical = 0;
    for (std::size_t i = 1; i < cuts.size(); ++i) {
        const auto a = cuts[i - 1], b = cuts[i];
        if (a == b)
            continue;
        TokenRole role = TokenRole::text;
        while (lexical < line.spans.size() && line.spans[lexical].start + line.spans[lexical].length <= a)
            ++lexical;
        if (lexical < line.spans.size() && line.spans[lexical].start <= a)
            role = line.spans[lexical].role;
        for (const auto &s : line.overlays)
            if (a >= s.start && a < s.start + s.length)
                role = s.role;
        const char *begin = line.text.data() + a, *end = line.text.data() + b;
        d->AddText(p, tokenColor(role, background), begin, end);
        p.x += ImGui::CalcTextSize(begin, end).x;
    }
}
CodeViewResult codeViewer(const char *id, CodeViewState &s, const CodeDocument &doc, ImVec2 size,
                          ArrayView<CodeAnnotation> annotations) {
    ScopedID scope(id);
    CodeViewResult result;
    if (s.searchedRevision != doc.revision() || s.searchedQuery != s.query) {
        s.matches.clear();
        if (!s.query.empty())
            for (std::size_t i = 0; i < doc.lines().size(); ++i)
                if (doc.lines()[i].text.find(s.query) != std::string::npos)
                    s.matches.push_back(int(i));
        s.searchedRevision = doc.revision();
        s.searchedQuery = s.query;
        s.match = s.matches.empty() ? -1 : 0;
    }
    ImGui::SetNextItemWidth(-dp(100));
    detail::inputText("##find", s.query);
    ImGui::SameLine();
    if (ghostButton("Next") && !s.matches.empty()) {
        s.match = (s.match + 1) % int(s.matches.size());
        s.focusLine = s.matches[std::size_t(s.match)];
    }
    if (size.y > 0)
        size.y = std::max(dp(1), size.y - ImGui::GetFrameHeightWithSpacing());
    ScopedFont font(monoFont());
    ScopedColor ground(ImGuiCol_ChildBg, palette().bg0);
    ImGui::BeginChild("##source", size, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    const float height = ImGui::GetTextLineHeightWithSpacing(), gutter = dp(56);
    const float width = std::max(ImGui::GetContentRegionAvail().x,
                                 gutter + float(doc.columns()) * ImGui::CalcTextSize("M").x);
    if (s.focusLine >= 0) {
        ImGui::SetScrollY(std::max(0.f, height * s.focusLine - ImGui::GetWindowHeight() * .25f));
        s.focusLine = -1;
    }
    ImGuiListClipper clip;
    clip.Begin(int(doc.lines().size()), height);
    while (clip.Step())
        for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i) {
            ScopedID row(i);
            const auto p = ImGui::GetCursorScreenPos();
            detail::listRow("##line", {width, height});
            auto *d = ImGui::GetWindowDrawList();
            if (i == s.selectedLine)
                d->AddRectFilled(p, {p.x + width, p.y + height}, palette().selection);
            if (ImGui::IsItemClicked()) {
                s.selectedLine = i;
                result.clickedLine = i;
            }
            char number[32];
            std::snprintf(number, sizeof(number), "%d", i + 1);
            d->AddText({p.x + dp(5), p.y}, palette().textFaint, number);
            paintCodeLine(d, {p.x + gutter, p.y}, doc.lines()[std::size_t(i)]);
            if (!s.query.empty() && doc.lines()[std::size_t(i)].text.find(s.query) != std::string::npos)
                d->AddRect({p.x + gutter, p.y}, {p.x + width, p.y + height}, palette().warning, 0, 0, dp(1));
            for (std::size_t j = 0; j < annotations.count; ++j)
                if (annotations[j].line == std::size_t(i)) {
                    d->AddCircleFilled({p.x + gutter - dp(8), p.y + height * .4f}, dp(3),
                                       toneColour(annotations[j].tone));
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", annotations[j].message.c_str());
                    if (ImGui::IsItemClicked())
                        result.annotation = annotations[j].id;
                }
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Copy line"))
                    ImGui::SetClipboardText(doc.lines()[std::size_t(i)].text.c_str());
                ImGui::EndPopup();
            }
        }
    ImGui::EndChild();
    return result;
}
} // namespace cyui
