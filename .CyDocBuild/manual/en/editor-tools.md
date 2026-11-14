---
id: editor-tools
title: Editor tools
description: Typed details, hierarchies, tables, file selection and shared commands.
section: Guides
order: 11
revision: 1
---

The reusable editor modules build on Core. Select `Inspector`, `Hierarchy`,
`DataView`, `FilePicker`, `Commands`, `Nodes` and `Timeline` as needed; model
ownership, resource access and Undo transactions stay in the application.

## Details inspector {#inspector}

Inspector supports bool, exact int64, double, float, vector2/3/4, RGBA color,
typed asset references and homogeneous, non-nested arrays of up to 1024 atoms.
Assign a stable nonzero ID and an untranslated `key` to each property. `category`
adds a category header. `defaultValue` enables reset; `readOnly`, bounds, choices
and the host validator apply to committed changes.

Right-click a field to copy/paste JSON; right-click a category to copy/paste all
its fields. Clipboard envelopes use `format:"cyui.inspector"`, `version:1`,
`kind:"property"` with a typed `property`, or `kind:"category"` with a `properties`
map. Categories match by property keys and types, independently of object IDs,
translated labels or category names. Vector dimensions and asset classes must
match. A category paste validates every field before returning commits; the host
applies them together as one Undo transaction. Read-only fields or validation
errors reject the whole category. Input is limited to 1 MiB and 32 JSON levels.

```cpp
cyui::Property strength;
strength.id = 1;
strength.key = "strength";
strength.label = "Strength";
strength.category = "Material";
strength.value = 2.56;
strength.bounded = true;
strength.minimum = 0;
strength.maximum = 10;
auto json = cyui::propertyToJson(strength);
auto paste = cyui::propertyFromJson(strength, json);
// Validate paste; apply its entire edits vector as one host transaction.
```

`commonProperties` combines fields from selected objects, marks differing values
as mixed, intersects bounds and respects read-only fields. Apply a commit to all
matching objects together. Keep one InspectorState per view and increment its
revision when replacing the model externally. The gallery's Inspector tab shows
both a material example and advanced properties with a two-object editing option.

## Numeric expressions {#expressions}

Double-click a number, focus it and press Enter, or select **Edit expression...**
from its context menu. `Value` is the value at edit start. Enter evaluates and
clamps the result; invalid arithmetic keeps the field open with an error.
For example, **2.56 → Value*100 → 10** when the upper bound is 10.

Core's typed numeric entry also supports expressions in drag fields, sliders,
audio controls and Inspector vector components. Supported syntax: `+ - * / % ^`,
parentheses, `Value`, `pi`, `e`, `k` suffix, and `abs`, `sqrt`, `min`, `max`,
`clamp`, `round`, `floor`, `ceil`. Use decimal dots and commas between function
arguments. Input is limited to 4096 bytes and 32 nested operations; division by
zero, unknown functions, non-finite results and code are rejected.

int64 clipboard values and integer literals preserve every bit. On platforms
where long double has double precision, expression arithmetic starting outside
±(2^53−1) is refused; use an exact integer literal there.

## Hierarchies and data tables {#views}

Hierarchy consumes flat parent/child records with root parent 0. It keeps matching
ancestors visible during search, supports Ctrl/Shift selection, rename, duplicate,
remove, visibility and locking. Drop onto a row to reparent; Shift-drop inserts
before it; the root zone moves to root. Cyclic or locked moves and host vetoes are
rejected. Requests leave the application model untouched until the host applies them.

DataView adds numeric multi-column sorting, filters, resizable/reorderable/hideable
columns and clipped rows. Cell types are text, int64, double and bool. Double-click
to edit; Enter commits and Escape cancels. Shift-click selects a cell rectangle;
Ctrl+C/V exchanges quoted TSV with tabs/newlines preserved. Numeric doubles accept
expressions and bounds. Paste validates all cells before returning a batch,
including read-only rows/columns and the application's validator. Changing the
revision cancels an active cell edit.

## Files and commands {#files-and-commands}

FilePicker renders a host-provided cached directory listing for open file(s), save
file or directory selection. Navigation produces a new directory request; the
application supplies asynchronous filesystem or virtual filesystem results.
Loading, stale and failed listings cannot be accepted. Extension filters are
case-insensitive, paths stay lexically under the configured root, and existing
save targets require confirmation. The application resolves OS permissions,
symlinks and final write-time checks and performs the actual operation.

CommandRegistry shares command metadata between menus, toolbars, palettes and
keyboard routing. The first active context wins before global commands. Typing
suppresses shortcuts unless explicitly allowed. Same-context collisions are
rejected; distinct contexts may reuse a chord. Rebind through shortcutEditor or
load a versioned `cyui.commands` JSON envelope. Loading is atomic. Poll once per
frame in the focused host window and execute the returned ID in the application.

## Graph and timeline editing {#graphs-and-timelines}

Nodes accepts comment groups and reroutes. Groups move/resize/rename, reroutes
move, and context menus create/remove overlays. Ctrl+C/V/D exchanges selected
nodes with internal links and reroutes. Graph JSON uses `cyui.nodes`, version 1.
Cloning allocates fresh IDs and remaps all internal references; unrelated links
are excluded. Apply created elements, committed overlays and removals in the host.

Timeline supports split at playhead (**S**), duplicate (**Ctrl+D**), marker editing
and lane-name drag reordering. The application supplies a fresh-ID allocator;
locked clips remain untouched and minimum durations are preserved. Markers and
lane order return separate edit vectors. Escape, read-only or a changed revision
cancel gestures. Engines and playback remain host responsibilities.

## Integrations and examples {#integrations}

The new Hierarchy, DataView and FilePicker modules depend only on Core and C++17.
Core now compiles `src/Numeric.cpp`; manual builds must also include optional
support units InspectorData, CommandRegistry, NodeData and TimelineData with their
modules. CMake and the Unreal packaging script select these sources automatically.

Python provides the corresponding modules, typed Inspector clipboard models,
command registry and graph/timeline helpers. They import without imgui-bundle;
drawing loads it lazily. Python uses PropertyFloat to distinguish float32 from
double. Native and Python typed JSON and binding envelopes are interchangeable.

Gallery tabs: Inspector **12**, Nodes **7**, Timeline **8**, Commands **16**,
Hierarchy **30**, Data view **31**, File picker **32**. Switch between all five
themes and both densities to check application-specific content.
