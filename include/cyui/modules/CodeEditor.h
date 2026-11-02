#pragma once
#include "cyui/modules/Code.h"
#include <memory>
class TextEditor;

namespace cyui {
// Optional MIT-licensed editor backend: multi-cursor, undo/redo, search, wrapping,
// language colorization, completion callbacks and annotations. Never reads a file.
class CodeEditor {
  public:
    CodeEditor();
    ~CodeEditor();
    CodeEditor(const CodeEditor &) = delete;
    CodeEditor &operator=(const CodeEditor &) = delete;
    void setText(const std::string &);
    std::string text() const;
    void setLanguage(CodeLanguage);
    void setReadOnly(bool);
    bool draw(const char *id, ImVec2 size = {}); // true when the document changed
    TextEditor &native();                        // advanced host hooks; include TextEditor.h when used
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace cyui
