---
id: module-recipes
title: Module API and recipes
description: Module API and recipes for CyUI integrators.
section: API
order: 21
revision: 2
---

All headers below use the `cyui/` prefix. Link the corresponding target described
in [MODULES.md](doc:modules). The examples for every widget are in Gallery.cpp,
tabs 4–19; the optional 3D adapter example appears in Viewport when enabled.
Use the same palette/dp/style rules as Core for host callbacks and overlays.

### IDs, state and edits {#ids-state-and-edits}

`Interaction.h` defines `ItemId` (uint64), `Point` (double x/y), `Range` (min/max),
`Rect` (screen bounds), borrowed `ArrayView<T>` and `Edit<T>`. Zero is reserved for
no object. `ItemScope` includes all 64 ID bits in the ImGui stack. Own one state
per panel, not one static gesture shared by every document. `snapValue` rounds
symmetrically; `zoomRange` keeps the pointer anchor fixed.

Interactive model editors return begin/update/commit/cancel proposals. Apply a
commit to the application model and record one Undo transaction. A revision
change cancels a stale active edit; advance it when the external model is replaced.
Atomic actions can commit immediately. Do not overwrite the external model with
all preview updates unless the application deliberately owns that preview layer.

```cpp
cyui::CurveState state;                   // persist with the panel
std::vector<cyui::CurvePoint> points;      // sorted, finite, stable nonzero IDs
cyui::CurveOptions options;
options.revision = document.revision;
options.snap = 0.25;
auto edit = cyui::curveEditor("##automation", state, points, cyui::dp(0, 260), options);
if (edit.committed()) document.commitCurve(edit.value);
```

The diagram/model types contain no engine pointers, JUCE strings or mandatory
JSON types. Keep arrays and callback captures alive for the duration of a draw.

### Canvas and numeric plots {#canvas-and-numeric-plots}

| API | Use |
| --- | --- |
| `CanvasView::world/screen/zoomAt/fit` | Double world coordinates and anchored transforms |
| `CanvasScope(id, state, size, options)` / `frame()` | Reserve a canvas, draw its background/grid and scope clipping; `frame().wheel` when navigation or `claimWheel` takes the wheel |
| `drawCanvasGrid(rect, view, step)` | Grid overlay in the current window draw list |
| `Axis::fraction/value` | Inverse linear/log mapping; logarithmic values require positive bounds |
| `plot(id, PlotState&, ArrayView<PlotSeries>, size, options)` | Line/step/points/bars, ticks, legend, pan/zoom, crosshair and nearest sample ID/index |
| `reducePlot(points, budget)` | Prepare min/max buckets, retaining spikes and conservative gaps |
| `histogram(values, range, bins)` | uint64 counts, inclusive endpoints; ignores NaN/outside values |
| `heatmap(id, values, columns, rows, valueRange, size)` | Bounded cells, missing-data color and hovered row/column |

Canvas: middle drag or Space+left pans, wheel zooms at the pointer. Plot: middle
drag pans, wheel zooms X, Ctrl-wheel also zooms Y. `PlotOptions` controls grid,
axes, legend, crosshair/navigation, maxPoints and optional formatX/formatY callbacks
for host units. Ticks and legend are drawn inside the plot bounds. Reported
`skippedSeries` must prompt host reduction/querying; oversized data is not silently
reduced every frame. Prepare/cache a bounded visible dataset outside draw.

```cpp
cyui::PlotState state{{{20, 20000}, true}, {{-90, 0}, false}};
cyui::PlotSeries line{1, "Spectrum", preparedPoints, cyui::PlotStyle::line, 1};
cyui::PlotOptions options;
options.formatX = [](double hz) { return formatFrequency(hz); };
auto hover = cyui::plot("##response", state, {&line, 1}, cyui::dp(0, 220), options);
```

### Curves, graphs and timeline {#curves-graphs-and-timeline}

| API | Use / result |
| --- | --- |
| `evaluateCurve(points, time, fallback)` | Linear, step, smooth or Bezier evaluation of sorted keys |
| `curveEditor(id, CurveState&, points, size, options)` | Editable points/handles and vector edit proposals |
| `arrangeNodes(nodes, links, spacing)` | Deterministic layered positions; returns proposals, tolerates cycles |
| `nodeEditor(id, NodeState&, nodes, links, size, options)` | Move vectors, connectFrom/connectTo, removeNodes/removeLink requests |
| `timeline(id, TimelineState&, lanes, clips, markers, size, options)` | Clip move/trim edits, seek, range selection and remove IDs |

Curve: double-click adds/removes keys; drag a point/selected Bezier handle; Ctrl
extends selection, box-select blank space; Ctrl+A/C/V, Delete, Escape are scoped
to the focused panel. Ctrl controls host/application shortcuts through ImGui's
focus state. CurveOptions.evaluate evaluates the preview with host interpolation;
snapTime can override the default snapping grid.

Nodes: drag cards or pins; Ctrl/Shift multi-select; blank drag box-selects;
Home frames the graph; Delete proposes removing selected nodes; right-click a
link in empty space proposes unlinking. `canConnect(from,to)` validates host pin
types and connection rules before emitting a connect request. `drawBody` draws
host details inside a card; it does not run the graph. The host removes dangling
links when deleting nodes and allocates IDs for accepted new links.

The gallery's image graph uses `NodeOptions::drawBody` and a taller `row` to draw
an image below a card's pin labels. Texture IDs come from the host through
`GalleryResources::imagePreviews` (four optional previews); the host must keep
them alive while the gallery draws. Assets uses the same textures as thumbnails,
and Inspector uses one as an example object preview. These resources are optional:
other hosts and tests show placeholders when no image is supplied. The demo's
generated image pixels are sample data, independent of UI palette colors.

Timeline: wheel zooms at the pointer, Shift-wheel pans, Ctrl-wheel scrolls lanes;
drag clips or their edges to move/trim. Locked clips/readOnly are respected.
Blank drag selects a time interval; the ruler requests seeking. TimelineOptions
supplies playhead, snap/minDuration, revision, formatTime, snapTime and drawClip.
Time units and frame/sample rounding remain the application's responsibility.

### Audio controls and prepared signal views {#audio-controls-and-prepared-signal-views}

`modules/Audio.h` retains knob/fader/meter and meterPosition. A fader spreads its
own minimumDb..maximumDb over its travel (size 0 or negative follows ImGui sizing).
The separate AudioViews target offers:

| API | Input |
| --- | --- |
| `makePeaks(samples, columns)` | Min/max envelope preparation, preserving missing-only buckets as NaN |
| `waveform(id, WaveformState&, WaveformData, playhead, size)` | Coverage interval plus prepared per-channel peak arrays |
| `spectrum(id, PlotState&, series, size[, PlotOptions])` | Frequency/value series, logarithmic X (invalid X resets to 20..20000) |
| `oscilloscope(id, PlotState&, series, size[, PlotOptions])` | Prepared time/amplitude series |
| `stereoScope(id, PlotState&, series, size[, PlotOptions])` | Square X/Y scope; both axes share one range symmetric around 0 |

Waveform emits seek/range-selection state, supports wheel zoom, Shift-wheel pan
and middle drag. Decode audio, query envelope caches and compute FFT/DSP on the
host side. Keep raw float audio ownership and thread synchronization out of draw.

### Audio analysis displays {#audio-analysis-displays}

These views belong to **AudioViews**, with the same dependency closure (`Plots`,
`Canvas`, `Core`); linking Audio controls, Curves or an audio SDK is unnecessary.
The native gallery's **Audio analysis** tab (22) and Python gallery show every view.

| C++ / Python API | Display and inputs |
| --- | --- |
| `spectrogram` | Time/frequency/intensity matrix, linear or logarithmic bins and frequency view, palette heat colors, intensity scale, crosshair and exact hovered frame/bin/value |
| `loudness` | Momentary, short-term and integrated LUFS, LRA in LU from absolute low/high LUFS bounds, configurable target marker and overlaid histories |
| `truePeak` / `true_peak` | Per-channel dBTP, held peaks, overs, warning/ceiling markers, scrollable channel list and reset request |
| `correlation` | Signed -1..+1 gauge and optional history; use `stereoScope` / `stereo_scope` alongside it for L/R or prepared M/S phase traces |
| `filterResponse` / `filter_response` | Overlaid Hz/dB gain curves, independent right-side degree axis for phase, editable stable-ID frequency/gain handles |
| `dynamics` | Input/output dB transfer curves, unity reference, current operating point and positive-dB gain-reduction bar |

```image
{"id":"audio-analysis","src":"assets/audio-analysis.webp","alt":"AudioViews gallery with spectrogram, editable EQ and phase, LUFS, dynamics, true peak and correlation","controls":"minimal"}
```

**Spectrogram layout:** `SpectrogramData.values[frame * bins + bin]` uses increasing
frames and bins over `time` and `frequency`. `logarithmicBins` describes stored bin
spacing, independently of `PlotState.y.logarithmic`. Values are dB by default;
`SpectrogramOptions.intensity/unit` configure the color range and displayed unit.
Initialize the view to the coverage, e.g. `PlotState{{{0, 8}, false}, {{20, 20000}, true}}`.
Wheel zooms time, Ctrl-wheel also zooms frequency, and middle drag pans both axes.
`SpectrogramResult` reports `valid`, `hovered`, `clicked`, `viewChanged`, `mouse`,
`frame`, `bin`, `intensity`. Malformed dimensions/ranges are rejected before indexing.
NaN cells stay blank. Rendering samples screen-cell centers with at most `maxCells`
(default 65,536, clamped to 1..262,144); it is not a peak-preserving reduction.
Prepare the desired visible resolution when small transients must remain visible.

**Meter values:** unavailable readings use NaN (`missingAudioValue`); negative
infinity represents silence. Supply LUFS, LRA bounds, dBTP holds/overs and history
series as measurements. `LoudnessOptions` and `TruePeakOptions` customize targets,
level ranges and the reset button. Reset returns `AudioMeterResult.resetRequested`
without changing input measurements. `clippedChannels` counts channels currently
above the ceiling or with recorded overs. The host decides which accumulated values
and histories to clear. Loudness/correlation history Y ranges remain fixed to their
meter scale; their X view supports plot navigation. Give history X a time range.

```cpp
cyui::ResponseState state; // persist per panel
std::vector<cyui::ResponsePoint> bands{{1, 120, 4}, {2, 1800, -7}};
cyui::ResponseOptions options;
options.revision = revision;
auto result = cyui::filterResponse("##eq", state, gainCurves, phaseCurves,
                                  bands, cyui::dp(0, 240), options);
if (result.points.committed()) commitBands(result.points.value); // one Undo entry
// During a drag, evaluate preview curves from state.preview.
```

EQ handles are Hz/dB positions, with unique nonzero 64-bit IDs (at most 1,024).
The result follows begin/update/commit/cancel; drag updates a preview, release
commits, Escape cancels. Changed revision, input model, read-only mode or lost
window focus cancels an active gesture. Invalid/duplicate handles disable editing.
Frequency/gain bounds constrain edits; plot navigation controls the visible view.
Phase series exceeding `PlotOptions.maxPoints` appear in `result.plot.skippedSeries`,
as gain series do. Gain/phase curves remain separately supplied, including previews.

Python uses the same data classes with snake_case fields: `SpectrogramData`,
`LoudnessData`, `TruePeakChannel`, `ResponsePoint`, `ResponseState`, `DynamicsData`.
Pass display options as keyword arguments. Results use snake_case dictionary keys;
`filter_response(...)["points"]` is an `Edit` with `.phase`, `.value`, `.committed`.
The `gain` curves argument uses a different name from the `gains=Range(-24, 24)`
editing bounds. `dynamics` expects a linear dB plot view, e.g. both axes -60..0;
`DynamicsOptions.reduction` / Python `reduction=` sets its attenuation scale.

### Code, editing and Git-style diff presentation {#code-editing-and-git-style-diff-presentation}

`CodeDocument::setText(text, language, revision)` preserves original UTF-8 bytes,
tabs, CRLF and final-newline state. Display lines expand tabs to four columns and
carry lexical spans. Languages include plain, C++, C#, Python, JavaScript,
TypeScript, Rust, GLSL, HLSL, JSON, Lua, SQL, INI and Markdown. Lexical coloring is a
presentation heuristic; supply semantic parser/LSP results as overlays when needed.

- `setOverlays(SourceSpan[], revision)` accepts zero-based lines and raw UTF-8
  **byte** offsets. Stale revisions and offsets inside a code point are rejected.
- `tokenColor(role, background)` maps token roles through the current palette,
  adjusting toward the text role for contrast. `paintCodeLine` draws prepared spans.
- `codeViewer(id, CodeViewState&, document, size, annotations)` provides line
  numbers, search/next, selected/focused line, annotation hover and navigation IDs.
- `CodeEditor::setText/text/setLanguage/setReadOnly/draw` wraps the optional MIT
  native TextEditor. draw returns document-changed. `native()` exposes advanced
  TextEditor hooks for completion, annotations, cursors, wrapping and Undo/Redo;
  include TextEditor.h and handle document storage/revisions in the host.
- `DiffDocument::setTexts(before, after, language, workBudget)` prepares a bounded
  comparison; `setUnified(text, language)` reads a unified patch. `rows`, `pairs`,
  `hunks`, `columns`, `revision`, `approximate` expose prepared presentation data.
- `diffViewer(id, DiffState&, document, size)` offers unified/side-by-side modes,
  one shared vertical canvas for both columns, a shared horizontal text offset
  with fixed line-number gutters, hunk/search navigation, inline changed ranges
  and old/new line navigation results. Final-newline-only changes are represented;
  CRLF and LF lines compare equal, with a notice row when line endings differ.
  `DiffState::scroll` reports the current vertical offset; assigning it requests
  a scroll. Hosts should call this viewer directly, without mirroring child-window
  offsets. See [DIFF.md](doc:diff) for details and native regression coverage.
- `expandTabs(text, tabSize)` widens tabs to the next stop as displayed;
  `displayOffset(raw, byteOffset, tabSize)` maps a raw byte offset to that display text.

```cpp
cyui::DiffDocument comparison;
comparison.setTexts(beforeText, afterText, cyui::CodeLanguage::cpp);
cyui::DiffState view;
auto clicked = cyui::diffViewer("##comparison", view, comparison, cyui::dp(0, 400));
if (clicked.newLine > 0) host.openNewDocumentAt(clicked.newLine);
```

Do not call setTexts on unchanged large documents every frame. Code and Diff do
not depend on Git, LSP, a compiler or CodeEditor. Host repository/authentication,
review comments, merge resolution and actions stay outside the renderer.

### Inspector and assets {#inspector-and-assets}

`Property` supports bool, int64, double, strings, choices, readOnly/password/
multiline flags and optional bounds. Set integerMinimum/integerMaximum for exact
integer bounds; do not convert int64 through double. `inspector(id, state,
properties, revision, validator)` returns PropertyEdit entries. The validator
returns an error string; invalid drafts remain visible without committing.

`assetBrowser(id, AssetState&, items, size)` uses AssetItem{id,parent,label,type,
thumbnail,folder,favorite}. State owns query, folder, selected IDs and grid/list
mode. It returns open/favorite/rename/remove requests. It performs no filesystem
operation. `CYUI_ASSET` carries the stable numeric ID for host drag/drop handling.
Folder parents and thumbnails are host data; querying/scanning/loading stays outside.

### Images, viewport and optional 3D gizmo {#images-viewport-and-optional-3d-gizmo}

| API | Use |
| --- | --- |
| `imageViewport(id, ImageState&, ImageData, size, ImageTool)` | Texture/UV crop, checkerboard, fit/zoom/pan, pixel hover and crop edit proposal |
| `sampleGradient(stops, position, linearLight)` | Sample unordered stops using sRGB or explicit linear-light interpolation |
| `gradientEditor(id, GradientState&, stops, size, linearLight, revision)` | Select/drag/add/remove stops; returns edits |
| `cameraPosition/cameraViewMatrix(OrbitCamera)` | Arbitrary up axis, column-major right-handed camera |
| `viewport(id, ViewportState&, ImageData, size, navigation)` | Aspect-preserving scene texture, UV picking, camera interaction |
| `transformGizmo(id, TransformState&, Transform2D, options)` | 2D move/scale/rotation overlay, projection/unprojection callbacks |
| `gizmo3D(id, Gizmo3DState&, matrix, view, projection, rect, options)` | Optional host ImGuizmo adapter for column-major scene transforms |

Image: wheel zooms, middle drag pans, double-click fits; the crop tool returns a
rectangle, and Escape restores the previous selection. Gradient: double-click
adds an interpolated stop, drag moves it, right-click removes a stop when more
than two remain. Host color editors change stop colors. Texture lifetime, color
management and document transforms remain host-owned.

The transform gizmo's handles are ImGui items: over a canvas (imageViewport,
viewport, CanvasScope) they take the hover and the click, so nothing underneath reacts.
Host items drawn over a canvas get the same by calling `markCanvasOverlay()` each frame.
Viewport: Alt-left drag orbits, middle drag pans, wheel changes distance. Host
renderers produce the texture; CyUI does not render a 3D scene. Transform2D rotation
is radians. Ctrl snaps rotation to 15-degree increments. Gizmo3DOptions selects
ImGuizmo operation/mode, snap, orthographic view, revision and enabled state.
Link CyUI::ImGuizmo only when a compatible host target is supplied.

### Commands, logs and documents {#commands-logs-and-documents}

- `rankCommands(commands, query)` matches every search word and returns stable
  ranked indices. `commandPalette(id, open, state, commands, size)` displays a modal
  palette with keyboard navigation and returns a requested command ID. Disabled
  entries do not execute; the host maps IDs to actions.
- `LogBuffer(capacity,maxBytes)` provides synchronized append/clear/snapshot.
  `logView(id, LogState&, buffer, size)` filters/follows bounded cached rows and
  returns a selected ID; LogState.levels masks debug/info/warning/error.
  `diagnosticList(id, rows, size)` displays source/line/column/message and returns
  a requested diagnostic ID. The host resolves source navigation.
- `Document::setMarkdown` caches a small renderer subset: headings, paragraphs,
  quotes, bullet/numbered items, rules, fenced code, links, inline code/emphasis
  and pipe tables. `documentView(id, state, document, size)` returns link/copy
  results. Links are returned to the host, not opened by the renderer.
- `transcript(id, TranscriptState&, messages, size)` skips offscreen messages it has
  measured, caches Documents by message
  ID, follows appended/streamed content when at the bottom, and reports copy/link
  requests. Messages contain id/role/content/time. Full CyMD rendering can use Web.

The Markdown subset does not interpret raw HTML or run scripts. It intentionally
uses the same Core widgets and Code colors. Native inline emphasis is a themed
presentation subset rather than a complete CommonMark layout engine.

### Optional Web / CEF {#optional-web-cef}

`webPanel`, WebBackend/WebPlacement/WebInput, WebPolicy/webOrigin, webThemeJson
and webEventJson form the engine-independent browser module. `createWebCEF`,
`webCEFApp` and `webCEFBrowser` provide the optional CEF implementation. The
browser draws in a host-owned texture and shares CyUI theme roles through CSS.
See [WEB.md](doc:web) for the complete lifecycle, native/Python recipes, TypeScript
bridge, CyMD iframe protocol, origin checks, process sandbox and OS packaging.
CEF is separate from Core and from the module's neutral API. The local validated
runtime is Windows; Linux/macOS build/deployment validation is explicitly pending.

Rules when using it:

- Pump `CefDoMessageLoopWork()` (Python `runtime.pump()`) outside the ImGui frame,
  before `NewFrame`. `WebCEFOptions::upload` and the optional `release` callback run
  inside it; `release(texture)` comes once after the browser closed: free that
  texture only after the frames that used it (Python: `create(..., release=)`).
- `external` events are page requests, never commands. Only
  `WebPolicy::externalSchemes` (default http/https) are reported; check
  `userGesture`, `mainFrame` and `sourceOrigin`, apply your own allow-list and ask
  the user before opening anything with the shell. Sub-frame popups without a
  gesture are dropped.
- `WebEventKind::dropped` (data = count) reports events discarded by the queue caps
  (`maxQueuedEvents`, `maxQueuedErrors` for errors/console) or by policy.
- `WebPanelOptions::focus` is one-shot (taken on a false -> true change). A focused
  panel owns the keys it forwards, so ImGui nav and other widgets do not act on them.
- Backend calls must run on the CEF UI thread; others are ignored. The supplied
  bridge, helper and demo run CEF unsandboxed on Windows (WEB.md "Process sandbox").

### Python examples {#python-examples}

```python
from cyui_modules.curves import CurvePoint, CurveState, curve_editor
from cyui_modules.common import Range

state = CurveState()
state.view.x.view = Range(0, 10)
state.view.y.view = Range(0, 1)
points = [CurvePoint(1, 0, .2), CurvePoint(2, 10, .8)]
# Each ImGui frame:
edit = curve_editor('automation', state, points, (0, 260), revision=revision)
if edit.committed:
    points = edit.value
```


Python optional APIs are snake_case equivalents in the packages listed in
[MODULES.md](doc:modules). Python source offsets for SourceSpan are raw UTF-8
bytes; displayed lexer spans use Python character indices. Do not mix those two
coordinate systems. `CodeEditor` in Python offers native ImGui multiline Undo
plus a syntax preview; C++ TextEditor advanced callbacks are not exposed there.
`CEFRuntime` is an explicit optional browser loader; importing model modules is
independent of imgui-bundle, NumPy, audio/video or browser engines.

```image
{"id": "module-recipes-view", "src": "assets/image-graph.webp", "alt": "Node cards with host-generated image previews", "controls": "minimal"}
```
