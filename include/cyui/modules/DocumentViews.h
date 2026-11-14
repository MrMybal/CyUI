#pragma once
#include "cyui/modules/Code.h"
#include "cyui/modules/Images.h"
#include <unordered_map>
#include <unordered_set>

namespace cyui {

struct DocumentAsset { std::string path; std::vector<unsigned char> bytes; };
enum class DocumentBlockKind { paragraph, heading, quote, list, code, rule, table };
enum class DocumentInlineKind { text, code, strong, emphasis, link, image, softBreak };
enum DocumentStyle : unsigned {
    documentBold = 1,
    documentItalic = 2,
    documentUnderline = 4,
    documentStrike = 8,
    documentHighlight = 16,
    documentSpoiler = 32,
    documentKeyboard = 64,
    documentMono = 128
};
// One inline run, parsed once by setMarkdown. `text` is shown verbatim; `target` is a link's URL.
struct DocumentInline {
    DocumentInlineKind kind = DocumentInlineKind::text;
    std::string text, target;
    bool spaced = false; // whitespace precedes it in the source
    unsigned style = 0;  // combinable styles; kind remains available for older consumers
    std::size_t sourceBegin = 0, sourceEnd = 0;
    ItemId spoiler = 0;
    std::string title;
    float width = 0, height = 0; // optional ![caption|widthxheight](source), in logical units
};
struct DocumentBlock {
    DocumentBlockKind kind = DocumentBlockKind::paragraph;
    int level = 0;
    std::string text, language;
    CodeDocument code;
    std::vector<std::vector<std::string>> rows;
    std::vector<DocumentInline> inlines;                         // paragraph and list text
    std::vector<std::vector<std::vector<DocumentInline>>> cells; // table rows, parsed
    int indent = 0, quoteDepth = 0;
    unsigned number = 0; // zero: bullet, otherwise ordered-list number
    bool task = false, checked = false;
    std::size_t taskOffset = 0, sourceBegin = 0, sourceEnd = 0;
    std::string anchor;
    std::vector<int> alignments; // 0 default, 1 left, 2 center, 3 right
};
class CYUI_API Document {
  public:
    bool setMarkdown(const std::string &);
    const std::string &markdown() const { return text_; }
    std::uint64_t revision() const { return revision_; }
    std::string plainText() const;
    const std::vector<DocumentBlock> &blocks() const { return blocks_; }

  private:
    std::string text_;
    std::vector<DocumentBlock> blocks_;
    std::uint64_t revision_ = 0;
};
enum class DocumentMediaKind { image, video, audio, attachment, web };
struct DocumentMedia {
    ItemId id = 0;
    DocumentMediaKind kind = DocumentMediaKind::image;
    std::string source, title;
    float width = 0, height = 0;
};
struct DocumentLinkPreview {
    std::string title, description, site;
    ImageData image;
    std::string embedSource;
    DocumentMediaKind kind = DocumentMediaKind::web;
};
// Providers run on the UI thread and must be nonblocking. Return cached resources;
// the host owns texture lifetimes, async loading, network policy and playback engines.
struct DocumentServices {
    std::function<ImageData(const std::string &)> image;
    std::function<DocumentLinkPreview(const std::string &)> preview;
    // Invoked only after activation, for video/audio/web. false shows the native fallback card.
    std::function<bool(const DocumentMedia &, ImVec2)> embed;
};
struct DocumentHit {
    Rect bounds;
    std::string text;
    std::size_t begin = 0, end = 0;
};
struct DocumentState {
    std::vector<CodeViewState> codeViews;
    std::unordered_set<ItemId> spoilers, activeEmbeds;
    std::vector<DocumentHit> hits;
    std::unordered_map<std::string, float> anchors;
    std::string jumpTo, enlargedImage, enlargedCaption;
    ImageState imageView;
    std::uint64_t revision = 0;
    std::size_t selectionBegin = 0, selectionEnd = 0;
    bool selecting = false;
    float scroll = 0, scrollMax = 0;
    ItemId resizingImage = 0;
    float resizingWidth = 0;
};
struct DocumentResult {
    std::string link;
    int copiedBlock = -1;
    std::size_t taskOffset = std::string::npos;
    bool taskChecked = false;
    std::string media, selection;
    std::string resizedImage;
    float resizedWidth = 0;
};
struct DocumentOptions {
    const DocumentServices *services = nullptr;
    bool tasks = true, selection = true, linkPreviews = true, imageZoom = true;
    bool breaks = true;
    float imageHeight = 280, embedHeight = 240;
    // Allows a host/editor to synchronize source and preview without comparing scroll ratios.
    std::size_t scrollToSource = std::string::npos;
    bool resizeImages = false;
};
CYUI_API DocumentResult documentView(const char *id, DocumentState &, const Document &, ImVec2 size = {});
CYUI_API DocumentResult documentView(const char *id, DocumentState &, const Document &, ImVec2 size,
                                     const DocumentOptions &);
CYUI_API DocumentMediaKind documentMediaKind(const std::string &source);
// Escapes data and emits portable HTML. No script, fetching, filesystem access or browser engine.
CYUI_API std::string documentHtml(const Document &, const std::string &title = "Document");
struct TranscriptMessage {
    ItemId id = 0;
    std::string role, content, time;
};
struct TranscriptRowCache {
    float height = 0;
    std::size_t signature = 0; // role and time
};
struct TranscriptState {
    std::unordered_map<ItemId, Document> documents;
    std::unordered_map<ItemId, DocumentState> views;
    bool follow = true;
    std::size_t count = 0;
    // Measured message heights: offscreen messages only reserve them. Remeasured when the layout
    // (width, font size, frame height, item spacing) or a message's role/time/content changes.
    std::unordered_map<ItemId, TranscriptRowCache> heights;
    ImVec4 layout{};
};
struct TranscriptResult {
    ItemId copy = 0; // message to copy, 0 for none: give messages nonzero IDs
    std::string link;
};
CYUI_API TranscriptResult transcript(const char *id, TranscriptState &, ArrayView<TranscriptMessage>,
                                     ImVec2 size = {});
CYUI_API TranscriptResult transcript(const char *id, TranscriptState &, ArrayView<TranscriptMessage>,
                                     ImVec2 size, const DocumentOptions &);
} // namespace cyui
