#include "cyui/modules/CodeEditor.h"
#include <TextEditor.h>

namespace cyui {
struct CodeEditor::Impl {
    TextEditor editor;
    TextEditor::Language custom;
    bool changed = false;
    std::string original, baseline;
    bool crlf = false;
};
CodeEditor::CodeEditor() : impl_(std::make_unique<Impl>()) {
    impl_->editor.SetInsertSpacesOnTabs(false);
    impl_->editor.SetTransactionCallback([this](const auto &) { impl_->changed = true; });
}
CodeEditor::~CodeEditor() = default;
void CodeEditor::setText(const std::string &value) {
    if (text() != value)
        impl_->editor.SetText(value);
    impl_->original = value;
    impl_->baseline = impl_->editor.GetText();
    impl_->crlf = value.find("\r\n") != std::string::npos;
    impl_->changed = false;
}
std::string CodeEditor::text() const {
    const auto current = impl_->editor.GetText();
    if (current == impl_->baseline)
        return impl_->original;
    if (!impl_->crlf)
        return current;
    std::string result;
    for (char c : current) {
        if (c == '\n')
            result += '\r';
        result += c;
    }
    return result;
}
TextEditor &CodeEditor::native() {
    return impl_->editor;
}
void CodeEditor::setReadOnly(bool value) {
    impl_->editor.SetReadOnlyEnabled(value);
}
void CodeEditor::setLanguage(CodeLanguage language) {
    const TextEditor::Language *definition = nullptr;
    switch (language) {
    case CodeLanguage::cpp:
        definition = TextEditor::Language::Cpp();
        break;
    case CodeLanguage::csharp:
        definition = TextEditor::Language::Cs();
        break;
    case CodeLanguage::python:
        definition = TextEditor::Language::Python();
        break;
    case CodeLanguage::glsl:
        definition = TextEditor::Language::Glsl();
        break;
    case CodeLanguage::hlsl:
        definition = TextEditor::Language::Hlsl();
        break;
    case CodeLanguage::json:
        definition = TextEditor::Language::Json();
        break;
    case CodeLanguage::lua:
        definition = TextEditor::Language::Lua();
        break;
    case CodeLanguage::sql:
        definition = TextEditor::Language::Sql();
        break;
    case CodeLanguage::markdown:
        definition = TextEditor::Language::Markdown();
        break;
    default:
        break;
    }
    if (language == CodeLanguage::javascript || language == CodeLanguage::typescript ||
        language == CodeLanguage::rust) {
        if (impl_->editor.GetLanguage() != &impl_->custom ||
            impl_->custom.name != (language == CodeLanguage::rust         ? "Rust"
                                   : language == CodeLanguage::typescript ? "TypeScript"
                                                                          : "JavaScript")) {
            impl_->custom = *TextEditor::Language::Cpp();
            impl_->custom.name = language == CodeLanguage::rust         ? "Rust"
                                 : language == CodeLanguage::typescript ? "TypeScript"
                                                                        : "JavaScript";
            for (const char *word :
                 {"let",    "const", "var",       "function", "async",      "await",   "export",
                  "import", "from",  "interface", "type",     "implements", "extends", "pub",
                  "fn",     "impl",  "trait",     "match",    "mut",        "self",    "Self"})
                impl_->custom.keywords.insert(word);
        }
        definition = &impl_->custom;
    }
    if (impl_->editor.GetLanguage() != definition)
        impl_->editor.SetLanguage(definition);
}
bool CodeEditor::draw(const char *id, ImVec2 size) {
    auto &p = impl_->editor;
    TextEditor::Palette colors;
    colors.fill(palette().text);
    using C = TextEditor::Color;
    auto put = [&](C kind, ImU32 color) { colors[std::size_t(kind)] = color; };
    put(C::keyword, tokenColor(TokenRole::keyword));
    put(C::declaration, tokenColor(TokenRole::type));
    put(C::number, tokenColor(TokenRole::number));
    put(C::string, tokenColor(TokenRole::string));
    put(C::preprocessor, tokenColor(TokenRole::preprocessor));
    put(C::comment, tokenColor(TokenRole::comment));
    put(C::knownIdentifier, tokenColor(TokenRole::type));
    put(C::background, palette().bg0);
    put(C::cursor, palette().text);
    put(C::selection, palette().selection);
    put(C::whitespace, palette().textFaint);
    put(C::lineNumber, palette().textFaint);
    put(C::currentLineNumber, palette().accent);
    put(C::currentLineHighlight, alpha(palette().accent, .08f));
    put(C::currentLineHighlightBorder, palette().line);
    put(C::matchingBracketBackground, palette().bg3);
    put(C::matchingBracketActive, palette().accent);
    put(C::matchingBracketLevel1, palette().info);
    put(C::matchingBracketLevel2, palette().success);
    put(C::matchingBracketLevel3, palette().warning);
    put(C::matchingBracketError, palette().error);
    p.SetPalette(colors);
    ScopedFont font(monoFont());
    p.Render(id, size);
    const bool changed = impl_->changed;
    impl_->changed = false;
    return changed;
}
} // namespace cyui
