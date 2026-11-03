# Native Diff viewer

Select `Diff` in `CYUI_MODULES`, include `<cyui/modules/Diff.h>` and link
`CyUI::Diff`. The module depends on Core and Code, with the host's existing ImGui
instance; it adds no platform backend, Git integration or editor implementation.

Prepare a `DiffDocument` with `setTexts` or `setUnified`, then retain a `DiffState`
per view and call `diffViewer(id, state, document, size)`. Its API and state/result
layout remain compatible with earlier versions. Source preparation stays in the
document; repeatedly drawing a prepared view does not recompute the diff.

Both split columns are now painted inside one child canvas, using exactly the
same visible row origin. Wheel/trackpad input over either column and the common
vertical scrollbar move that canvas once. There is no next-frame scroll mirroring
between independent panes. Paired blank rows retain the alignment of additions
and removals. Fixed row spacing agrees with the clipper's row height, preserving
line navigation in long, scrolled comparisons.

Long source lines use one horizontal text offset in both columns. Their line
numbers remain fixed and code/inline highlights clip inside each column. Column
widths follow the actual visible canvas, so long text cannot push the other
column offscreen. Unified mode uses the same canvas. Hunk/search navigation,
selected rows, translated labels, source colors and Copy line remain available.

`DiffState::scroll` reports the current vertical position after input. Assigning
a different value restores that position on the next call; `focus` continues to
request row/hunk navigation. Distinct widget IDs keep independent canvas storage.
Changing layouts does not create a second ImGui child or duplicate input handling.

`cyui_diff_scrolling` tests the actual native widget without an OS window: a
single child, wheel input over either column, vertical scrollbar dragging, shared
horizontal scrolling, clicked source-line numbers, explicit offset restoration,
focus navigation and empty documents. It draws split/unified layouts in all five
themes, both densities, EN/FR and 100/150/200% DPI. The ordinary feature and syntax
tests also cover document preparation. The Windows demo's Code / Diff gallery tab
(11) uses this same implementation. Windows/MSVC evidence does not establish
Linux/macOS execution. The independent legacy Python renderer is outside this
native C++ correction.
