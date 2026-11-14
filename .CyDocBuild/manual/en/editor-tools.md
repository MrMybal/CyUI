---
id: editor-tools
title: Editor tools
description: Typed details, hierarchies, tables, file selection and shared commands.
section: Guides
order: 11
revision: 3
---

The reusable editor modules build on Core. Select `Inspector`, `Hierarchy`,
`DataView`, `FilePicker`, `Commands`, `Nodes` and `Timeline` as needed; model
ownership, resource access and Undo transactions stay in the application.

## Details inspector {#inspector}

Inspector supports bool, exact int64, double, float, vector2/3/4, RGBA color,
typed asset references, recursive arrays, Sets, Maps, optional values, enums/flags,
typed records and custom JSON values.
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
errors reject the whole category. Import and export are limited to 1 MiB, 32 JSON
levels and 65536 nodes. These clipboard budgets are independent of view size.

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

### Virtual arrays and custom types {#virtual-arrays}

Array rows reuse every property editor: bool, exact int64, float/double expressions,
text/choices, vectors, colors, asset references, arrays, records and custom values.
Only visible rows and the active offscreen row are submitted. The old 1024-element
display limit is removed (row indices go up to INT_MAX). Nested arrays and records
open detail popups, keeping row heights constant. Right-click an element to copy or
paste typed JSON, duplicate, move up/down or remove it. Add uses the element template
or the last value. The gallery's **Virtual arrays** tab shows up to 100000 elements.

```cpp
cyui::Property points;
points.id = 10;
points.label = "Points";
points.value = cyui::makePropertyArray(cyui::PropertyVector{{0,0,0}}, 10000);
cyui::PropertyObject spawn{"SpawnPoint", {
    {"Position", cyui::PropertyVector{{1,2,3}}},
    {"Weights", cyui::makePropertyArray(.5f, 3)}
}};
points.value = cyui::makePropertyArray(spawn, 10000);
```

`makePropertyArray(elementTemplate,count=0)` preserves the element schema even when
empty and supplies Add defaults. Legacy `PropertyArray{{values...}}` infers its
schema from the first element; an untyped empty array adds a string. Records use
stable field keys and a stable type name. Nesting is limited to 12 container levels.
Parent bounds/choices apply recursively; the host validator checks the whole edit.

`PropertyCustom{type,json}` adapts application-specific types with strict JSON data.
Use `InspectorOptions::editCustom(id,path,value)` to return an `Edit<PropertyCustom>`
with begin/update/commit/cancel phases. Draw one frame-height row using the current
item width. The fallback is a JSON editor. `browseAssetAt(id,path)` identifies nested
resource fields. Paths contain array indices and object keys. Application models,
Undo and resource loading remain host-owned. This is an explicit adapter, not
automatic reflection of arbitrary C++ types.

`arrayHeight` (default 180) uses logical units. `InspectorState::lists` exposes the
last frame's `renderedRows` count. Large drafts are cached: increment revision for
every external model replacement/change, including while idle. Full model copying
and validation happen on edits rather than each idle frame.

Python has matching `PropertyObject`, `PropertyCustom`, `make_property_array` and
`inspector(..., array_height=180, edit_custom=..., browse_asset=...)` APIs. Numeric
elements accept expressions directly with Enter. Its callback returns `Edit` and
receives the same root ID/path. The JSON schema remains version 1 with optional
array `element` templates and `object`/`objectType`, `custom`/`customType` entries.

### Sets, Maps and bulk editing {#collections}

`makePropertySet(elementTemplate)` and `makePropertyMap(keyTemplate,valueTemplate)`
keep their schema when empty. A Set stores unique typed elements in `values`; a Map
stores `{key,value}` entries in `entries`. Every property type can be an element,
key or value, including recursive records and custom types. Their meaning ignores
entry order while the view keeps insertion order. Uniqueness is checked after
normalization/clamping; collisions reject the complete edit or paste. Add opens a
draft editor so users can supply a unique element/key before insertion.

```cpp
auto tags = cyui::makePropertySet(std::string("Tag"));
tags.values = {std::string("Player"), std::string("Visible")};
auto weights = cyui::makePropertyMap(std::string("Key"), .5f);
weights.entries = {{std::string("Walk"), .7f}};
auto optional = cyui::makePropertyOptional(std::int64_t(0)); // absent
cyui::PropertyEnum quality{"Quality", 1, {{0,"Low"},{1,"Medium"},{2,"High"}}};
cyui::PropertyEnum flags{"Channels", 3, {{1,"R"},{2,"G"},{4,"B"}}, true};
```

Arrays, Sets and Maps share virtual rows, search and Ctrl/Shift multiselection.
Ctrl+A selects filtered rows; Delete removes the selection. **Edit selection**
applies a numeric expression or typed property JSON atomically; `Value` refers to
each original element. Map bulk edits affect values. Arrays also duplicate and
reorder selections by dragging their row indices. `editPropertyCollection` exposes
these operations without drawing. The Sets/Maps gallery pages support 100000 rows.

`PropertyObject::metadata[fieldKey]` supplies `PropertyMetadata` for each field:
label/help, read-only, bounds (including exact int64 bounds), choices, password and
multiline. Metadata belongs to the destination and is never replaced by pasted
JSON. Array structural edits retain read-only record fields. Optional values keep
an immutable template and nullable owned value; enum type/items and allowed flag
bits are checked. `propertyEditor` embeds any editor in another host view with the
same validation and transaction phases.

JSON version 1 adds `set` with typed elements and an optional `element` template;
`map` with `value:[{"key":typed,"value":typed}]`, `keyTemplate` and `valueTemplate`;
`optional` with a null or typed `value` and `element`; and `enum`/`flags` with
`enumType`, int64 `value` and `{value,label}` items. Clipboard budgets are unchanged.
Python mirrors these types/factories and `edit_property_collection`; Map entries
are `(key,value)` pairs, and metadata bounds use `minimum`/`maximum` or `None`.
The standalone editor is `property_value_editor`.

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

Arrow keys navigate visible rows: Right expands/enters children and Left
collapses/selects the parent. Home/End/PageUp/PageDown, Ctrl/Shift selection,
Space and Ctrl+A work with filtering. F2/Enter requests rename; Delete requests
removal. `navigateHierarchy` is available without a renderer.

DataView adds numeric multi-column sorting, filters, resizable/reorderable/hideable
columns and clipped rows. `DataCell` is `PropertyValue`, so every Inspector type
works, including vectors/colors/assets, collections, optionals, enums/flags,
records and custom types. Double-click opens the corresponding editor.
`DataColumn::metadata` supplies rules; `DataViewOptions::editors` supplies custom
and resource callbacks (column ID, with row ID first in the path). Shift-click
selects a cell rectangle; Ctrl+C/V exchanges quoted TSV with tabs/newlines
preserved. Rich cells contain typed property JSON; primitives retain plain text.
Numeric cells accept expressions and bounds. Paste validates all cells before returning a batch,
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

Group movement also moves nodes whose anchors were inside its original rectangle
and their contained reroutes. Apply `groups`, `moved` and `reroutes` commits as one
transaction. Escape/revision cancels all previews. `moveGroupContents=false`
disables this behavior; group resizing leaves contents fixed. Python has parity.

Timeline supports split at playhead (**S**), duplicate (**Ctrl+D**), marker editing
and lane-name drag reordering. The application supplies a fresh-ID allocator;
locked clips remain untouched and minimum durations are preserved. Markers and
lane order return separate edit vectors. Escape, read-only or a changed revision
cancel gestures. Engines and playback remain host responsibilities.
The context menu adds markers in native and Python views; Python's
`add_timeline_marker` helper also works without drawing.

## Integrations and examples {#integrations}

Hierarchy and FilePicker depend on Core; DataView also depends on Inspector to
reuse its editors. All use C++17.
Core now compiles `src/Numeric.cpp`; manual builds must also include optional
support units InspectorData, CommandRegistry, NodeData and TimelineData with their
modules. CMake and the Unreal packaging script select these sources automatically.

Python provides the corresponding modules, typed Inspector clipboard models,
command registry and graph/timeline helpers. They import without imgui-bundle;
drawing loads it lazily. Python uses PropertyFloat to distinguish float32 from
double. Native and Python typed JSON and binding envelopes are interchangeable.

Gallery tabs: Inspector **12**, Nodes **7**, Timeline **8**, Commands **16**,
Hierarchy **30**, Data view **31**, File picker **32**, Virtual arrays **33**,
Sets **34**, Maps **35**, Rich data view **36**, Editor workspace **37**. Switch between all five
themes and both densities to check application-specific content.

Editor workspace connects Hierarchy, Inspector, DataView and the asset picker to
one host model and shared selection, with Ctrl+Z/Ctrl+Y Undo/Redo. The reusable
`include/cyui/examples/EditorWorkspace.h` host validates an entire change batch
before updating its model/history. Adapt its snapshots to your application's
Undo system; resource access remains host-owned.
