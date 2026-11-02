# CyUI modules

CyUI provides a shared interface foundation plus optional libraries. An application
selects its modules at build time. Its documents, engines and services remain owned
by the application.

## Available now

| Target | Header | Contents |
| --- | --- | --- |
| `CyUI::Core` | `cyui/Core.h` | Theme, DPI, fonts, icons, ordinary widgets, value entry, notifications and application layout |
| `CyUI::Audio` | `cyui/modules/Audio.h` | Existing knobs, faders and peak/RMS meters; depends only on Core |
| `CyUI::Gallery` | `cyui/Gallery.h` | Widget gallery and theme editor; Core only, with audio examples when Audio is enabled |
| `CyUI::CyUI` | `cyui/CyUI.h` | Compatibility umbrella linking the modules enabled in this build |

`cyui/Widgets.h` preserves the original combined widget declarations. Core-only
applications use `cyui/Core.h` or `cyui/CoreWidgets.h`. Existing function names
and widget behavior are preserved. This is source compatibility; static library
files are now separate and manually maintained linker inputs must be updated.

Audio and Gallery are enabled by default to preserve existing integrations.
Linking Core does not link either module. To avoid building them as well:

```cmake
set(CYUI_IMGUI_TARGET my_imgui_target)
set(CYUI_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_GALLERY OFF CACHE BOOL "" FORCE)
add_subdirectory(external/CyUI)
target_link_libraries(my_app PRIVATE CyUI::Core)
```

An audio application enables Audio and links `CyUI::Audio`, which also links Core:

```cpp
#include <cyui/Core.h>
#include <cyui/modules/Audio.h>
```

`CYUI_HAS_AUDIO` and `CYUI_HAS_GALLERY` are public compile definitions on their
respective CMake targets. They describe the modules linked to the consumer. They
are not runtime plugin discovery. The complete Win32 demo requires both modules.

Without CMake, compile `Theme.cpp`, `Widgets.cpp` and `Layout.cpp` for Core, add
`src/modules/Audio.cpp` for Audio, and `src/Gallery.cpp` for Gallery. Define
`CYUI_HAS_AUDIO=1` when compiling Gallery to include its audio examples.

The existing Python port remains `python/cyui.py`. It currently provides the
general UI foundation, not these C++ audio controls. Python module extraction and
packaging remain separate work; the CMake split does not add Python features.

## Application survey, 2026-11-02

The survey covers first-party interface source trees and their build dependencies,
with focused inspection of the components below. Generated output, installed
runtimes and bundled dependencies are excluded from the feature inventory.
This is a source survey, not execution or validation of those applications.

Paths in this table are relative to each named application's repository.

| Application | Relevant source examples | Reusable presentation | Keep in the application |
| --- | --- | --- | --- |
| CyDAW | `src/v2/CurveEditor.*`, `DeviceVisuals.*`, `GraphEditor.*`, `GraphLayout.*`, `ArrangementView.cpp`, `AudioEditorView.cpp` | Editable automation curves, graph cards and links, timelines, waveform and device displays | JUCE bindings, audio taps, FFT/DSP, effect catalogue, automation interpolation and project Undo |
| Shaderixel | `src/ui/NodeEditor.cpp`, `CodeEdit.*`, `GradientEditor.cpp`, `Analysis.cpp`, `Canvas.cpp` | Node canvas, annotated shader code, gradient stops, histograms, image scopes, zoom and overlays | Shader compilation, effect graph evaluation, document model, GPU device and image-processing algorithms |
| ShaderixFrame | `src/ui/Timeline.cpp`, `CurveEditor.cpp`, `LayerCanvas.*`, `DspCanvas.*`, `CommandPalette.cpp`, `src/core/CanvasView.h`, `src/adapters/CyDawWidgets.*` | Clip lanes, ruler, markers, keyframes, graph navigation and command search | Video decoding, rendering, frame rate/timecode policy, adapters, project history and DSP graph |
| CyGameSDK | `app/native/GraphView.cpp`, `Content.cpp`, `Viewport.cpp`, `Gizmo.cpp`, `Editor.cpp` | Typed pin presentation, asset cards/tree, property forms, viewport surface and gizmo adapter | Graph interpreter/types, asset registry, scene model, validation, engine adapters and game formats |
| CyCode / CyCodeReader | `app/src/EditorTheme.cpp`, `EditorLanguage.cpp`, `Syntax.cpp`, `HighlightUI.cpp`, `TerminalUI.cpp`, `VcsUI.cpp`, `apps/CyCodeReader/Reader.cpp` | Code editor/viewer, token colors, diagnostics, gutters, search, diff presentation and terminal surface | Document revisions, LSP/DAP clients, language servers, compilers, process execution, VCS and terminal process |
| CyTools | AudioLibrary `gui/waveform_view.py`; AudioStudio `gui/timeline.py`, `effect_ui.py`, `knobs.py`; MeshStudio `gui/widgets.py`; AIChat `gui/chat_panel.py`; Storyboard `gui/layout_editor.py`; ComfyFlow `result_viewer.py` | Peak envelope view, audio plots, timelines, schema forms, result cards, image/video surfaces, transcript and viewport presentation | Audio decoding/cache storage, inference, providers, model installation, jobs, permissions, media decoding and offscreen rendering |
| CyInsights | `App/InsightsViews.cpp`, `CounterViews.cpp`, `AnalysisViews.cpp`, `OvrHeatmapView.hpp`, `ActivityViews.cpp`, `ComparisonViews.cpp` | Timing tracks, overview/range selection, sampled counter plots, histograms, heatmaps and comparison tables | Trace readers/indexes, aggregation, capture, counter precision/availability, GPU measurements and background jobs |
| CyRevision | `native/CyRevision.Native/TextDocument.*`, `TextViewer.cpp`, `GitViews.cpp`, `GitConflictViews.cpp`, `NativeWidgets.cpp` | Unified/split diffs, synchronized scrolling, hunk navigation, lazy syntax spans, commit lanes and descriptor forms | Git/review/merge actions, server state, network/authentication and repository history queries |

CyTools also includes Manager, MiniMaxH3, PersonaRP, Subtitle, VideoToGaussian and
VideoToStereo3D. Their common forms, progress, previews, logs and update panels
reinforce the same families. Product-specific model and installation behavior
does not belong in CyUI. The PersonaRP Colab tree is a packaging variant.

ShaderixFrame already has explicit CyDAW widget and Shaderixel graph adaptations.
Those are evidence of repeated UI work, but also of coupling: the complete editor
classes cannot be moved into CyUI with their current engine/document dependencies.
CyRevision adds an optional Diff component to the Code family. Code reading,
editing and comparison should share token roles and line/document views while
remaining selectable independently.

## Modules to extract next

Only Core, Audio controls and Gallery are implemented as targets today. Names in
this table are proposed boundaries, not available CMake targets or completed ports.

| Priority | Proposed module | Shared responsibility | Likely consumers |
| --- | --- | --- | --- |
| 1 | Canvas | Per-view pan/zoom, coordinate transforms, clipping, grid, hit tests and selection rectangles | Node, curve, image and timeline views |
| 1 | Plots | Numeric axes, linear/log mappings, sampled line/step plots, histograms, heatmaps, hover/crosshair | CyDAW, AudioStudio, Shaderixel, ShaderixFrame, CyInsights |
| 2 | Curves | Keyframe drawing, points/handles, snapping, selection and gesture events; host-supplied interpolation | CyDAW, ShaderixFrame, Shaderixel, AudioStudio |
| 2 | Nodes | Cards, typed pin visuals, links, selection, move/connect proposals and navigation; host-supplied bodies | CyDAW, Shaderixel, ShaderixFrame, CyGameSDK |
| 2 | Timeline | Ruler, lanes, intervals/clips, markers, playhead, overview, range selection, move/trim gestures | CyDAW, ShaderixFrame, AudioStudio, Storyboard, Subtitle, CyInsights |
| 2 | AudioViews | Peak-envelope waveform, scope, spectrum, stereo field and response-curve presentation | CyDAW, AudioLibrary, AudioStudio, ShaderixFrame |
| 2 | Code | Source view/editor adapter, syntax roles, token overlays, gutters, diagnostics and search | CyCode, Reader, Shaderixel, AudioStudio JSFX editor |
| 2 | Code / Diff | Optional comparison component: unified/split changed lines, synchronized scrolling, hunk navigation, inline ranges and comments | CyRevision, CyCode, graph/text comparison panels |
| 3 | Inspector | Typed/schema-driven property forms, validation messages and edit proposals | CyGameSDK, CyRevision, CyTools, graph inspectors |
| 3 | Assets | Search/filter views, tree/grid/cards, thumbnails, selection and drag/drop descriptors | CyGameSDK, CyDAW, Shaderixel, CyTools libraries |
| 3 | Images | Texture viewport, checkerboard, fit/zoom, crop/transform overlays and gradient-stop editor | Shaderixel, ShaderixFrame, Storyboard, result viewers |
| 3 | Viewport | Texture surface, camera interaction and optional gizmo integration | CyGameSDK, Storyboard, MeshStudio, Shaderixel |
| 3 | Commands / Diagnostics | Searchable command presentation; bounded/filterable log and diagnostic views | Editors and CyTools |
| 4 | DocumentViews | Transcript, Markdown and code-block presentation, after identifying a shared renderer contract | AIChat, PersonaRP, Storyboard |

Audio controls remain separate from AudioViews: a knob or meter does not require
FFT, waveform decoding or a plotting backend. A timing profiler is a consumer of
Timeline and Plots; it does not make every timeline depend on trace processing.
Likewise, a node graph is not a numeric plot and a gradient is document color data,
not a second hard-coded UI theme.

Canvas and Plots should be extracted first because several later modules need
their coordinate, interaction and clipping contracts. The Code family can proceed
independently of that canvas stack; Diff must not require a full editor or a Git
backend. Optional adapters for TextEditor, Tree-sitter,
ImGuizmo or platform file dialogs must have their own dependencies and explicit
enablement; they must not enter Core or download packages during a frame.

## Extraction contract

- Use the host's ImGui target and context. No renderer, event loop, windowing
  toolkit or platform backend is imposed by a module.
- Keep stable IDs and interaction state per view/context. Do not share a static
  selected point, drag operation or camera between unrelated views.
- Accept lightweight views/callbacks over host data. Avoid JUCE strings, engine
  types, repository paths, SDK jobs or a mandatory JSON library in public Core APIs.
- Report begin/update/commit/cancel gestures or edit proposals. The host performs
  validation and records one Undo transaction per committed gesture. Rendering
  does not bypass revision checks or mutate a live engine directly.
- Preserve host interpolation, time units, logarithmic ranges, missing-data
  semantics and integer counter precision. Do not silently reinterpret them.
- Draw only visible rows and bounded samples. Waveform providers return min/max
  envelopes for the visible resolution; trace providers query the visible range.
  Decoding, FFT, indexing, cache files and worker synchronization stay outside draw.
- Take UI colors from palette roles and sizes from `dp()`/style. Semantic token
  roles and data-series colors need explicit theme mappings. Preserve readability
  in all five themes and both densities.
- Keep localization strings supplied by the host and keep user content unchanged.
  Python modules must use the same concepts without adding NumPy, PyAV or a 3D
  engine to `import cyui`.
- Track provenance before transferring source. Several surveyed projects declare
  GPL/AGPL licenses and CyCode's instructions leave its project license to the
  owner. This change moves only existing CyUI code under its existing MIT license;
  no application implementation is incorporated by the survey.

For each subsequent module, include a working Gallery example, API documentation,
isolated build/link coverage, representative interaction checks, and an explicit
adoption change in each consuming application. Consumers using copied CyUI source
do not receive updates automatically.

## Checking the available modules

```sh
cmake -S . -B build/modules -DCYUI_BUILD_DEMO=OFF -DCYUI_BUILD_TESTS=ON
cmake --build build/modules --config Release
ctest --test-dir build/modules -C Release --output-on-failure
```

Repeat with Audio/Gallery both disabled, each enabled separately, and both enabled.
The tests resolve Core, module and compatibility symbols against their selected
libraries. `build_demo.bat` continues to build the full visual gallery.
