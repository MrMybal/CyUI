#pragma once
#include "cyui/modules/Code.h"
#include <unordered_map>

namespace cyui {
enum class DocumentBlockKind { paragraph, heading, quote, list, code, rule, table };
struct DocumentBlock {
    DocumentBlockKind kind = DocumentBlockKind::paragraph;
    int level = 0;
    std::string text, language;
    CodeDocument code;
    std::vector<std::vector<std::string>> rows;
};
class CYUI_API Document {
  public:
    bool setMarkdown(const std::string &);
    const std::vector<DocumentBlock> &blocks() const {
        return blocks_;
    }

  private:
    std::string text_;
    std::vector<DocumentBlock> blocks_;
};
struct DocumentState {
    std::vector<CodeViewState> codeViews;
};
struct DocumentResult {
    std::string link;
    int copiedBlock = -1;
};
CYUI_API DocumentResult documentView(const char *id, DocumentState &, const Document &, ImVec2 size = {});
struct TranscriptMessage {
    ItemId id = 0;
    std::string role, content, time;
};
struct TranscriptState {
    std::unordered_map<ItemId, Document> documents;
    std::unordered_map<ItemId, DocumentState> views;
    bool follow = true;
    std::size_t count = 0;
};
struct TranscriptResult {
    ItemId copy = 0;
    std::string link;
};
CYUI_API TranscriptResult transcript(const char *id, TranscriptState &, ArrayView<TranscriptMessage>,
                            ImVec2 size = {});
} // namespace cyui
