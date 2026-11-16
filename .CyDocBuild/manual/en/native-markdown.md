---
id: native-markdown
title: Native Markdown, editing and media
description: Compose native documents, chats and editors with optional resource adapters.
section: Guides
order: 17
revision: 7
---

## Try the standalone playground {#playground}

The SDL3 application shares its sources on Windows, Linux and macOS. Build with
CMake using `CYUI_BUILD_MARKDOWN_DEMO=ON` and `CYUI_BUILD_DEMO=OFF`, then run
`cyui_markdown_test`. On Windows, `build_markdown_demo.bat` provides a shortcut
with Visual Studio 2022 and CMake, producing `build\cyui_markdown_test.exe`.
`CYUI_BUILD_PORTABLE_DEMO=ON` also builds the SDL3 widget gallery `cyui_demo_sdl3`.
SDL3 is example-only: installed >= 3.4, otherwise a pinned SHA-256-verified fetch;
`CYUI_FETCH_DEMO_DEPENDENCIES=OFF` requires a local installation.
The chat uses Enter to send, Shift+Enter for a new line, small hover/context
copy/edit/reply/delete actions, and inline editing (Enter saves, Escape cancels).
Use +, file-manager drop, or Ctrl+V (Cmd+V on macOS) to attach real files/images. Attachment-only
messages work. Images enlarge in a native zoom view; Markdown/text attachments
open in the document editor; the download icon saves a file.

The document defaults to Live direct rich editing, with compact/floating
formatting, Source, Side by side and Reading modes. Images can be pasted,
inserted or dropped, and relative paths resolve from the document's folder.
Saving creates plain `.md` with referenced media in `assets/`; no CyMD package
is needed. Ctrl+O opens, Ctrl+S saves, Ctrl+Shift+S saves as. English/French,
five themes and both densities are available. The local chat is temporary.

Image import uses portable stb codecs (PNG/JPEG/BMP/TGA/PSD/GIF; first GIF frame).
SDL3 handles windows, native rendering, file dialogs and drops. Clipboard images,
HTML and copied files use SDL MIME data and isolated Windows/macOS adapters;
macOS TIFF screenshots are converted to PNG. Paths and arguments remain UTF-8.
File dialogs are asynchronous and their results reach app state on the UI thread.
Web/CEF, YouTube, remote image fetching and link previews are optional separate
services, with no runtime dependency here. `--self-test` checks messages,
edit/cancel/reply, image decoding and a real document-plus-image file round trip.
`python tests/capture_markdown_demo.py` captures the native views.
CMake users can enable `CYUI_BUILD_MARKDOWN_DEMO=ON`.

## Native message interaction {#chat}

`TranscriptAttachment` carries a stable ID, name, source, byte count and image flag.
The host owns the bytes and supplies cached textures through `DocumentServices`.
`TranscriptMessage::own=false` hides edit/delete; `edited` displays its status and
`replyTo` links to another stable message ID. Hover/right-click actions return
requests in `TranscriptResult`: `edit`, `erase`, `reply`, `attachmentMessage`,
`attachmentIndex` and `download`. The renderer never edits host messages or opens
files. Chat supports Discord underline `__text__`; document Markdown semantics
stay unchanged and code spans/fences retain literal underscores.

```cpp
cyui::MessageComposerState input;
// Keep this state between frames. The host imports clipboard files/images.
cyui::MessageComposerOptions options;
options.canSubmitEmpty = !pendingAttachments.empty();
auto action = cyui::messageComposer("message", input, {}, options);
if (action.submitted) {
    sendLocalMessage(input.text, pendingAttachments);
    input.text.clear();
    input.focus = true;
}
```

`paste(replacement)` returns true when the host handled rich paste; an empty
replacement is valid after importing attachments. `TranscriptOptions::editing`
and `editor(message,width)` draw a host-owned editor inside a message row.
Use the same composer with `editing=true`; save/cancel only on the returned
requests. Formatting and native text selection remain available. No file dialog,
network transport or browser engine is pulled into the reusable module.

## Virtualize large conversations {#virtualization}

`transcript` renders and parses only visible message rows. Variable heights use
a sparse index with logarithmic lookup/update; unseen rows start with an estimate.
Logical scrolling uses double precision and a 64-bit scrollbar, with local drawing
coordinates. The parsed-document/interaction cache defaults to 128 messages
(`cacheMessages`), plus visible rows and pinned editors/open images. Height metadata
contains no message text. A single visible message is still rendered as one document.

For a paged history, use `TranscriptSource` instead of allocating the whole message array:

```cpp
cyui::TranscriptSource history;
history.id = conversationId;
history.count = store.messageCount();
history.revision = store.orderRevision();
history.message = [&](std::size_t row) { return pageCache.find(row); }; // pointer or nullptr
history.request = [&](std::size_t first, std::size_t count) { queuePageRead(first, count); };
history.indexOf = [&](cyui::ItemId id) { return store.indexOf(id); }; // indexed lookup, npos if absent
cyui::transcript("history", state, history, {0, 500});
```

Callbacks run on the UI thread: queue I/O and publish loaded pages from the host.
Unloaded rows show placeholders. `prefetchRows` requests a margin around the viewport.
Increment `revision` after prepend/reorder/removal; appending only changes `count`.
`scrollToIndex`, `scrollTo`, `jumpToBottom` and `follow` control navigation; public
`firstVisible`/`lastVisible` and `fetched` expose the processed window. An uncached
reply parent retains its ID jump link. The array overload uses the same virtual renderer;
only an explicit ID jump may scan its array. Supply an indexed `indexOf` for large stores.

The gallery includes an on-demand million-message history. The standalone app can
open the same stress case with `--scenario million-chat`, without allocating that history.

## Choose the components {#components}

The document reader and editor draw native Dear ImGui widgets and textures.
Neither needs a browser. Select only the targets your application uses:

| Target | Purpose | Dependencies |
| --- | --- | --- |
| `CyUI::DocumentViews` | Markdown reader and streaming transcripts | Code, Images, MIT MD4C parser |
| `CyUI::DocumentEditor` | Live, source, split and reading modes | DocumentViews, CodeEditor, DataGrid |
| `CyUI::DocumentIO` | In-memory `.md`/`.cymd`, clipboard HTML conversion | DocumentViews, MIT miniz inflate |
| `CyUI::Media` | Native audio/video playback controls | Images; application playback backend |
| `CyUI::DocumentWeb` | Optional document embed adapter | DocumentViews, Web; application browser backend |

```cmake
set(CYUI_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_GALLERY OFF CACHE BOOL "" FORCE)
set(CYUI_MODULES "DocumentEditor;DocumentIO" CACHE STRING "" FORCE)
add_subdirectory(external/CyUI)
target_link_libraries(app PRIVATE CyUI::DocumentEditor CyUI::DocumentIO)
```

This selection does not compile Web, CEF, Media or load a codec. Rebuild consumers
after updating the headers: the expanded models change the native binary ABI.
The APIs use UTF-8 byte positions at codepoint boundaries. Keep one state per view.

## Read and interact {#reader}

```image
{"id":"native-reader","src":"assets/markdown-reader.webp","alt":"Native Markdown reader, image, tasks, table and chat transcript","controls":"minimal"}
```

`Document::setMarkdown` parses only changed text. The reader supports nested
emphasis, bold, underline (`<u>`), strikethrough, `==highlight==`/`<mark>`, `<kbd>`,
inline/fenced/indented code, quotes, ordered/nested lists, GFM tasks, aligned
tables with escaped pipes, reference links, heading anchors, bare HTTP(S) URLs,
numeric/named entities and Discord-style `||spoilers||`. Unrecognized HTML is
shown as text. It does not implement HTML/CSS layout or execute scripts.

```cpp
#include <cyui/modules/DocumentViews.h>
cyui::Document document;
cyui::DocumentState view;
document.setMarkdown("# Notes\n\n- [ ] Review\n\n||Reveal me||\n");
auto result = cyui::documentView("notes", view, document, {0, 420});
if (result.taskOffset != std::string::npos) {
    auto source = document.markdown();
    source[result.taskOffset] = result.taskChecked ? 'x' : ' ';
    document.setMarkdown(source);
}
// result.link and result.media are requests for application policy/actions.
```

Drag text to select it and use Ctrl+C to copy visible text. Spoilers stay out of
the selectable text until revealed. Internal links navigate to the generated
heading anchor; `DocumentState::jumpTo` also requests an anchor. The state exposes
reading positions. Tasks return exact source offsets, rather than silently
changing an application's document. Transcripts reuse this renderer and cache
messages by their stable IDs, including streamed Markdown.

## Native images and optional embeds {#resources}

```image
{"id":"native-image-zoom","src":"assets/markdown-zoom.webp","alt":"An image enlarged in CyUI's native viewport inside the application","controls":"minimal"}
```

`DocumentServices::image(source)` returns cached `ImageData`. PNG, JPEG, SVG, GIF
or another format is decoded by the application's native image pipeline; CyUI
draws the resulting texture itself. An animated image may return a changing
frame. No HTML image element or browser is involved. The host owns texture
lifetimes and loading; the provider runs on the UI thread and must not block.

```cpp
cyui::DocumentServices services;
services.image = [&](const std::string& source) {
    return cachedNativeImages.resolve(source); // application-owned ImageData
};
cyui::DocumentOptions options;
options.services = &services;
auto result = cyui::documentView("notes", view, document, {0, 420}, options);
```

Images preserve their aspect ratio. `![caption|320x180](assets/photo.png)` supplies
logical size bounds. Activate an image to enlarge it inside the application;
the native viewport supports zoom/pan, and Escape closes it. The editable Live
view includes a width grip. A read-only view can enable `resizeImages` and handle
the returned `resizedImage`/`resizedWidth` proposal itself.

Image syntax pointing to a known audio/video extension creates a native fallback
card. `DocumentServices::embed` is called only after user activation. A missing
backend leaves the fallback usable. Cached `DocumentLinkPreview` objects supply
title, description, thumbnail and optionally an embed URL/kind. Angle-bracket
URLs suppress preview cards. Providers control resource access, network policy
and cache storage; the renderer does not fetch remote content.

`mediaPlayer` accepts a `MediaBackend`: snapshots provide frames and playback
state, and native controls request play/pause, seek, mute and volume. The same
backend may be used elsewhere in the app. It does not decode a file or open an
audio device. Seek is committed on release.

For web embeds, assign `services.embed` to a `DocumentWebAdapter::draw` callback.
Its `backend` callback returns an existing `WebBackend` for the stable media ID.
This may be a CyUI CEF view or the application's own adapter. The adapter navigates
on URL/backend changes and reports web events. The application owns policy,
backend lifetime, initialization and frame pumping, including hidden panels;
follow the [Web lifecycle](doc:web#native-lifecycle). No second engine is created.

## Edit a document {#editor}

```image
{"id":"native-live-editor","src":"assets/markdown-live.webp","alt":"Live native editor with styled text, editable tasks and a native image","controls":"minimal"}
```

```image
{"id":"native-split-editor","src":"assets/markdown-split.webp","alt":"Markdown source beside the native rendered document","controls":"minimal"}
```

```cpp
#include <cyui/modules/DocumentEditor.h>
cyui::DocumentEditor editor;
editor.setMarkdown("# Notes\n\nSome **formatted text**.\n");
cyui::DocumentEditorOptions options;
options.document.services = &services;
if (editor.draw("editor", {0, 560}, options).changed)
    markDocumentDirty(editor.markdown());
```

Live uses native text inputs with Markdown markers hidden and styled glyphs
overlaid at the input's own wrapping/selection positions. Images and tables are
native elements. Code blocks are editable with lexical colors. Tables use the shared native DataGrid:
edit cells directly, select/copy/paste ranges, and insert/remove rows or columns
from the context menu. Spreadsheet headers and the grid toolbar are hidden in documents. Cell input accepts inline Markdown; Reading
renders that formatting. Table changes share the source undo history. Source uses CodeEditor; Split adds a source-position-synchronized
preview. Reading uses the same reader. Switching modes preserves the source.
All native editing modes share the source editor's transaction history.

The main/floating toolbars and commands cover bold, italic, underline, strike,
spoiler, inline code, links, headings, lists, tasks, quotes and indentation.
`select(first,last)`, `insert(markdown)` and `command(...)` are available to host
menus. The editor reports changes instead of saving files. Ctrl/Cmd+1–4 changes
mode; common formatting shortcuts and find/replace are available. Live edits
normalize the edited block's Markdown representation, while other blocks remain
untouched. Ctrl+A selects the whole document, and dragging can cross paragraph
boundaries. Enter creates paragraphs/continues lists; Shift+Enter inserts a line
break inside the same block, preserving its style. Live writes these breaks as
`<br>` so blank lines survive parsing, focus changes, saving and reloading, including
headings and list items. Native layout includes the final empty line and uses the
same wrapping as keyboard input; the caret is the native input caret.
Typing keeps the caret collapsed, and the floating formatting
toolbar opens only for a text selection. Live paragraphs have no input-field outline.
Backspace joins adjacent
paragraphs and arrows move the caret between them. Typed Markdown is recognized
in Live. `focus()` requests native keyboard focus at the selected source caret. Blank fenced blocks, empty list items and
code nested in quotes/lists are opened in Source to preserve their structure.

`pasteRich` imports clipboard HTML/images through host services and returns
Markdown referencing registered resources. Ctrl+Shift+V keeps the normal plain
paste path. Pasting an HTTP(S) URL over a selection creates a link. `drop` accepts
the chosen ImGui payload type and returns imported Markdown; the application
converts OS file-drop events to its payload or calls `insert` directly. No OS
clipboard backend or file manager is loaded by this module.

`spelling(markdown, language)` is an optional cached provider. It returns UTF-8
source ranges and suggestions. Live underlines those ranges; the suggestions menu
applies replacements through the same undo history. Set `spellingRevision` when
the provider's results/dictionary change. No dictionary is bundled or selected
silently. Read-only mode disables changes and suggestions.

## Files, clipboard and HTML export {#io}

`decodeMarkdown`/`encodeMarkdown` preserve UTF-8/UTF-16 LE/BE, BOM and the source's
line endings. `readCyMD`/`writeCyMD` handle the `document.md`, `assets/`,
`previews.json` archive layout. ZIP imports accept stored/DEFLATE entries; exports
use stored entries. The interfaces consume/return memory buffers and never extract
files to disk. Keep the returned assets and preview cache with the document.

Default limits: 64 MiB archive, 128 MiB expanded, 32 MiB per entry, 4096 entries.
Traversal, duplicate paths, encrypted/unsupported entries, symlinks, checksum errors
and budget violations are rejected. ZIP64 is unsupported. `documentAssetPath`
helps validate host resource keys; still apply the application's filesystem policy.

`markdownFromHtml` converts common clipboard formatting, links, paragraphs,
lists, simple tables and code. Scripts/styles are discarded. Its explicit image importer must
register pasted resources. It is a clipboard conversion helper, not an HTML/CSS
renderer. `documentHtml` exports escaped portable HTML without scripts or remote
fetching; relative media references remain relative and must be distributed with it.
`documentPackageHtml(package, title)` instead embeds referenced package assets as
data URLs for a self-contained HTML file. Archive metadata is preserved in `manifest`.

## Python and validation {#python}

Python provides `documents`, `documenteditor`, `documentio`, `documentweb` and
`media`. Parsing uses the pure Python MIT `markdown-it-py` and `linkify-it-py`
dependencies; model imports do not load imgui-bundle, CEF or playback engines.
Resource services and backend contracts mirror the C++ architecture. Python
positions exposed by the document/editor models are UTF-8 byte offsets. Advanced
C++ TextEditor callbacks and source-editor capabilities remain C++ APIs. The
Python editor supports basic block editing, the four views and shared history;
floating toolbars, spelling, image resizing, source find/replace, synchronized
scrolling and reader drag-selection are currently C++ features.

The gallery shows the reader/transcript at tab 18, editor at tab 24 and a clearly
identified simulated playback backend at tab 25. The CI matrix builds and runs
the native apps on Windows/Linux/macOS (software rendering; Linux uses Xvfb).
Local verification covers Windows. Real Linux/macOS desktop interaction, native
IME, clipboard, dialogs, scaling and external engines need target validation;
the configured CI alone does not establish a successful run.

See [grids, paginated documents and Office formats](doc:office) for ODT/ODS and DOCX/XLSX support.
