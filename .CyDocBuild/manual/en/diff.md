---
id: diff
title: Native Diff viewer
description: Native Diff viewer for CyUI integrators.
section: Guides
parent: code
order: 31
revision: 1
---

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

```image
{"id": "diff-view", "src": "assets/diff.webp", "alt": "Aligned split comparison", "controls": "minimal"}
```
