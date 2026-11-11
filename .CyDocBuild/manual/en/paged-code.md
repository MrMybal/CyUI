---
id: paged-code
title: Paged code editor
description: Reusable virtual editing with host-owned storage, revisions and transactions.
section: Guides
parent: modules
order: 31
revision: 1
---

`CyUI::Code` owns `CodePaging.h`: UTF-16 positions, bounded UTF-8 fragments,
validation and a streaming line builder. `CyUI::CodeEditor` owns
`PagedCodeEditor.h`, also included by `CodeEditor.h`. It draws and edits a virtual
document without passing a page to the whole-document TextEditor backend.
There is no dependency on CyCodeCore, a filesystem, MCP or a language server.

## Select the component {#integration}

```cmake
set(CYUI_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_GALLERY OFF CACHE BOOL "" FORCE)
set(CYUI_MODULES "CodeEditor" CACHE STRING "" FORCE)
set(CYUI_IMGUI_TARGET app_imgui)
add_subdirectory(external/CyUI)
target_link_libraries(app PRIVATE CyUI::CodeEditor)
```

Use C++20 and one compatible ImGui instance. Code alone remains C++17. No extra
runtime dependency is introduced. Unreal source packaging includes the new
sources when CodeEditor is selected. This change supplies native C++ APIs;
Python/Web virtual-editor bindings are not provided.

## Host and view contract {#contract}

Keep one `PagedCodeEditor` per view on its ImGui/UI thread. Supply a
`PagedCodeDocument` with a stable nonzero document ID, nonzero content revision,
logical line count and read-only state. An empty source has one logical line;
a final newline adds an empty final line. Revisions are equality guards, not
timestamps. A different document/revision cancels stale reads and interactions.

Request and edit IDs are local to each view. Route asynchronous responses and
acknowledgements to the originating view; include an application view key when
batching requests from multiple editors of the same document.

`draw` returns a `PagedCodeRead` once when a new viewport needs data. The request
contains its own ID, document/revision guards, a line interval, display-cell
window, tab width and byte budget. Prepare it outside drawing, optionally in an
application-owned worker. Deliver `supplyPage` on the UI thread. A response from
an old request, document or revision is rejected. `cancelledRead` lets the host
cancel its own superseded task. No worker or I/O is started by the widget.

Return one `PagedCodeLine` for every requested logical line, with full-line
UTF-16/display-cell metrics and only its bounded visible fragment. The validator
checks identity, coverage, UTF-8 boundaries, token spans and budgets. Missing or
failed data is shown as loading/unavailable, with explicit Retry. The view uses
64-bit logical scroll origins and normalized scrollbars rather than an enormous
floating-point ImGui canvas; precise line navigation remains integer-based.

```cpp
cyui::PagedCodeEditor view; // persist with this panel
view.setDocument({documentId, contentRevision, logicalLines, 0, readOnly});
// Each frame:
auto result = view.draw("##source", cyui::dp(0, 400));
if (result.read) host.queueVisiblePage(*result.read);
if (result.edit.committed()) host.applyGuardedReplacement(result.edit.value);
for (const auto& action : result.actions) host.handleCodeAction(action);
// Completed host reads are delivered to view.supplyPage(...) on the UI thread.
```

These host names illustrate responsibilities, not another required library.
Use `pendingRead`, `failRead` and `retryRead` for an asynchronous adapter.

## Coordinates and long lines {#coordinates}

Positions are zero-based logical lines and UTF-16 code units. They are distinct
from UTF-8 byte offsets, display cells and tabs. Fragments exclude line endings
and an initial file BOM; the host preserves all original source bytes. Tokens
use fragment-local UTF-8 byte offsets and semantic palette roles.

`PagedCodeLineBuilder` accepts arbitrary UTF-8 chunk boundaries while scanning a
single logical line. It retains only the requested horizontal fragment and
calculates total UTF-16/display-cell metrics. It can resolve a requested UTF-16
position to `revealedColumn()` / `revealedByte()`, rejecting a split surrogate.
A position past a shortened line clamps to its end. For a `reveal` read, retain
the requested horizontal window when it already shows that position; otherwise
return a window containing it and set `page.firstColumn` accordingly.

Display uses one cell per BMP scalar, two per supplementary scalar and configured
tab stops. The default tab size is four. Wide/combining-script shaping, bidi,
grapheme-cluster motion and soft wrapping are not implemented. The monospace font,
palette roles, per-view Ctrl-wheel zoom and host DPI/style are shared with Code.

The byte budget applies to each page, not the whole file: at most 4 MiB, 4,096
requested rows, 8,192 display columns, 64 KiB per line fragment and 65,536 tokens.
Input intents are bounded to 256 events and the configured insertion budget
(1 MiB by default, at most 4 MiB). Exhaustion is explicit; it does not turn a
partial fragment into a complete source document.

## Edits and Undo ownership {#transactions}

Typing, paste, Enter/Tab and deletion produce atomic `EditPhase::commit`
replacement proposals containing the stable document ID, expected revision,
ordered UTF-16 selection and UTF-8 replacement. The widget never changes the
application's model or saves a file. Validate the proposal and commit one native
Undo transaction, then call `acceptEdit(id, newSnapshot, resultingSelection)`.
The acknowledged revision must differ. Reject failure through `rejectEdit`.

Typing is queued behind an asynchronous edit; it is not discarded on each
waiting frame. Rejection or an external revision cancels those intents and
returns `cancelledInput` for host recovery, plus the cancelled edit proposal.
Do not replay them silently on a different document. Complete or explicitly
resolve pending input before destroying a view. After an external edit, the host
may restore its rebased selection with `setSelection`.

Mouse selection reports begin/update/commit/cancel, with Escape cancellation.
Basic arrows, Home/End, Page Up/Down and Ctrl/Cmd+A/Home/End are supported.
Copy/Cut, Undo/Redo, Save and Find/next/previous are guarded host actions: the host
can read selections across uncached pages, perform global search and own its
clipboard/history. Copy the exact guarded selection before committing Cut.
No second text Undo stack or viewport-only global search is created.
Use `keyboardShortcuts=false` and `requestAction` for application keymaps.

For CyCodeCore, the application adapter translates these UTF-16 ranges to the
Core's guarded byte/line operations and native Apply/Undo owner. It must supply
page/line metrics and actual syntax context. The Core stays independent of CyUI.
No Core/IDE/Reader adapter or paged LSP overlay is installed by this library change.

## Syntax and example {#syntax}

Supply token roles prepared with the real lexical/semantic context. Lexing an
isolated fragment cannot infer an earlier multiline comment or embedded language.
The existing `highlightCode` whole-snapshot limit and API are unchanged; this view
does not secretly load or re-lex the entire source. Multiple cursors, folding,
completion menus and word-level navigation are not part of this first paged view.

Gallery's Code page switches between the existing in-memory editor and **Paged
editor**. Its small editable host demonstrates transactions, selection, clipboard
and host Undo/Redo. A separate read-only million-line source generates only
requested rows, including a streamed long line. It is synthetic demonstration
data, not a benchmark of a million-line disk file.

```text
build/cyui_demo.exe --gallery-only --tab 10 --paged-code --capture paged.bmp --theme navy
```

The standalone fixture's editable model is limited to 128 KiB; that is an example
storage policy, not a document-size limit in the paged widget.
