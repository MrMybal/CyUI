#pragma once
#include "cyui/Core.h"
#include "cyui/Interaction.h"
#include <string>
#include <string_view>

namespace cyui {
enum class CodeLanguage {
    plain,
    cpp,
    csharp,
    python,
    javascript,
    typescript,
    rust,
    glsl,
    hlsl,
    json,
    ini,
    sql,
    lua,
    markdown
};
enum class TokenRole { text, keyword, type, number, string, comment, preprocessor, function };
struct CodeSpan {
    std::size_t start = 0, length = 0;
    TokenRole role = TokenRole::text;
};
struct CodeLine {
    std::string text;
    std::vector<CodeSpan> spans, overlays;
};
struct SourceSpan {
    std::size_t line = 0, startByte = 0, endByte = 0;
    TokenRole role = TokenRole::text;
};
CYUI_API std::size_t displayOffset(std::string_view raw, std::size_t byteOffset, unsigned tabSize = 4);
CYUI_API std::string expandTabs(std::string_view, unsigned tabSize = 4);
class CYUI_API CodeDocument {
  public:
    bool setText(const std::string &, CodeLanguage language = CodeLanguage::plain,
                 std::uint64_t revision = 0);
    bool setOverlays(ArrayView<SourceSpan>, std::uint64_t revision);
    const std::string &text() const {
        return text_;
    }
    const std::vector<CodeLine> &lines() const {
        return lines_;
    }
    std::uint64_t revision() const {
        return revision_;
    }
    CodeLanguage language() const {
        return language_;
    }
    std::size_t columns() const {
        return columns_;
    }

  private:
    std::string text_;
    std::vector<CodeLine> lines_;
    std::uint64_t revision_ = 0;
    CodeLanguage language_ = CodeLanguage::plain;
    std::size_t columns_ = 0;
};
CYUI_API ImU32 tokenColor(TokenRole, ImU32 background = 0);
CYUI_API void paintCodeLine(ImDrawList *, ImVec2, const CodeLine &, ImU32 background = 0);
struct CodeAnnotation {
    ItemId id = 0;
    std::size_t line = 0;
    Tone tone = Tone::error;
    std::string message;
};
struct CodeViewState {
    std::string query;
    int match = -1, focusLine = -1, selectedLine = -1;
    std::vector<int> matches;
    std::uint64_t searchedRevision = 0;
    std::string searchedQuery;
};
struct CodeViewResult {
    int clickedLine = -1;
    ItemId annotation = 0;
};
CYUI_API CodeViewResult codeViewer(const char *id, CodeViewState &, const CodeDocument &, ImVec2 size = {},
                          ArrayView<CodeAnnotation> annotations = {});
} // namespace cyui
