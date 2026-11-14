---
id: native-markdown
title: Native Markdown, editing and media
description: Compose native documents, chats and editors with optional resource adapters.
section: Guides
order: 17
revision: 2
---

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
from the toolbar or context menu. Cell input accepts inline Markdown; Reading
renders that formatting. Table changes share the source undo history. Source uses CodeEditor; Split adds a source-position-synchronized
preview. Reading uses the same reader. Switching modes preserves the source.
All native editing modes share the source editor's transaction history.

The main/floating toolbars and commands cover bold, italic, underline, strike,
spoiler, inline code, links, headings, lists, tasks, quotes and indentation.
`select(first,last)`, `insert(markdown)` and `command(...)` are available to host
menus. The editor reports changes instead of saving files. Ctrl/Cmd+1–4 changes
mode; common formatting shortcuts and find/replace are available. Live edits
normalize the edited block's Markdown representation, while other blocks remain
untouched. A Live text selection belongs to its block; use Source for edits and
selections crossing block boundaries. Blank fenced blocks, empty list items and
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
identified simulated playback backend at tab 25. Windows builds and headless
interaction tests cover the native implementation. Linux/macOS runtime, native
IME platform integration and external engine integrations require validation in
the target application; portable source alone does not establish those results.

See [grids, paginated documents and Office formats](doc:office) for ODT/ODS and DOCX/XLSX support.
