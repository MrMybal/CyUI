---
id: modules
title: Select optional modules
description: Select optional modules for CyUI integrators.
section: Guides
order: 10
revision: 2
---

CyUI 1.2 provides a common Dear ImGui foundation and independently selectable
interface modules. All CyUI code is MIT licensed. Applications own their models,
engines, files, services, validation and Undo history.

## Localization in every integration {#localization-in-every-integration}

Core includes English (default), French and extensible translation catalogs.
Use stable translated labels when migrating application interfaces. C++, Python,
Web and Unreal usage, font requirements and host responsibilities are described
in [LOCALIZATION.md](doc:localization).

## Targets and dependencies {#targets-and-dependencies}

`CyUI::Core` contains themes, localization, quick filters, fonts, icons, ordinary widgets, layout and
notifications. Modules link Core automatically. The following targets are implemented:

Menu/panel filtering is opt-in through Core's `TextFilter`, `filterBox`,
`filterMenu` and the filtered panel header. Inspector accepts an optional filter
without adding a dependency. See [FILTERING.md](doc:filtering).

| CMake target | Public header | Interface | Additional CyUI dependency |
| --- | --- | --- | --- |
| `CyUI::Audio` | `modules/Audio.h` | Knobs, faders, peak/RMS meters | Core |
| `CyUI::Canvas` | `modules/Canvas.h` | Double coordinates, grid, clipping, pan/zoom | Core |
| `CyUI::Plots` | `modules/Plots.h` | Linear/log axes, lines, steps, points, bars, histograms, heatmaps | Canvas |
| `CyUI::Curves` | `modules/Curves.h` | Keyframes, Bezier handles, selection, snap, copy/paste | Plots |
| `CyUI::Nodes` | `modules/Nodes.h` | Cards, pins, links, layout, move/connect/remove requests | Canvas |
| `CyUI::Timeline` | `modules/Timeline.h` | Lanes, clips, markers, ruler, move/trim, range selection | Canvas |
| `CyUI::AudioViews` | `modules/AudioViews.h` | Waveforms, spectrum, oscilloscope, stereo scope | Plots |
| `CyUI::Code` | `modules/Code.h` | Paged line contracts/builders, source viewer, 102 syntax modes, extensible registry, semantic overlays, annotations, search | Core + selected Lexilla engines + system threads |
| `CyUI::Diff` | `modules/Diff.h` | Unified/split diff, line numbers, hunk navigation, inline ranges | Code |
| `CyUI::CodeEditor` | `modules/CodeEditor.h` | In-memory TextEditor and host-backed PagedCodeEditor (distinct capabilities) | Code + MIT TextEditor, C++20 |
| `CyUI::Inspector` | `modules/Inspector.h` | Typed properties, exact int64, validation, draft edits | Core |
| `CyUI::Assets` | `modules/Assets.h` | Hierarchical folder navigation, grid/list, search, thumbnails, drag/drop | Core |
| `CyUI::Images` | `modules/Images.h` | Texture viewport, checkerboard, crop, gradient stops | Canvas |
| `CyUI::Viewport` | `modules/Viewport.h` | Host-rendered scene surface, orbit camera, 2D transform overlay | Images |
| `CyUI::Commands` | `modules/Commands.h` | Searchable keyboard command palette | Core |
| `CyUI::Diagnostics` | `modules/Diagnostics.h` | Bounded thread-safe logs, filters, diagnostic navigation | Core + system threads |
| `CyUI::DocumentViews` | `modules/DocumentViews.h` | Markdown subset, fenced code, links, tables, cached transcripts | Code |
| `CyUI::Web` | `modules/Web.h` | Browser panel, texture/input contract, origin policy, messages, theme bridge | Core |
| `CyUI::Gallery` | `Gallery.h` | Examples and theme editor | Enabled modules only |

Header paths are relative to `include/cyui/`. `CyUI::Code::Diff` is an alias for
`CyUI::Diff`. `CyUI::CyUI` is the compatibility umbrella linking all modules
selected in this build. A module is a static library, not a dynamically loaded plugin.

Optional external adapters:

| Target | Enablement | Dependency |
| --- | --- | --- |
| `CyUI::ImGuizmo` | Set `CYUI_IMGUIZMO_TARGET` to an existing host target; select Viewport | Host's MIT ImGuizmo; `adapters/ImGuizmo.h` |
| `CyUI::WebCEF` | `CYUI_BUILD_WEB_CEF=ON`; select Web | Host CEF SDK/runtime; `adapters/WebCEF.h`, C++20 |

Neither adapter is downloaded or enabled by `CYUI_MODULES=All`. The Web module
is usable with a host backend; the supplied browser implementation is CEF.
See [WEB.md](doc:web) for platform support, process lifecycle and deployment.
WebCEF hands the host a texture `release` callback, reports `external` requests
with `userGesture`/`mainFrame`/`sourceOrigin` (schemes limited by
`WebPolicy::externalSchemes`) and counts discarded events in `dropped` events.
The supplied bridge and helper run CEF unsandboxed on Windows (see WEB.md).

The optional [Unreal Editor module](doc:unreal) packages selected CyUI modules for
embedding in an existing plugin's Source/CyUI folder or sharing through one provider
plugin. It provides a Slate renderer/input backend without menus or demo windows.
Prepare it separately with `tools/package_unreal.py --layout module` (default);
it is not a CMake target or a dependency of Core. A redistributed plugin embeds it
under a unique name with `--module-name`, or depends on the shared provider.

## Select only what an application needs {#select-only-what-an-application-needs}

```cmake
set(CYUI_IMGUI_TARGET app_imgui)
set(CYUI_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_GALLERY OFF CACHE BOOL "" FORCE)
set(CYUI_MODULES "Code;Diff;Inspector" CACHE STRING "" FORCE)
add_subdirectory(external/CyUI)
target_link_libraries(app PRIVATE CyUI::Diff CyUI::Inspector)
```

```cpp
#include <cyui/Core.h>
#include <cyui/modules/Diff.h>
#include <cyui/modules/Inspector.h>
```

Dependencies are selected automatically: Diff enables Code; it does not enable
CodeEditor, audio, plots or a browser. Link explicit targets to keep public feature
macros and dependencies restricted to the consumer. `CYUI_HAS_CODE`,
`CYUI_HAS_DIFF`, etc. are public definitions of their respective targets.

`CYUI_MODULES` accepts a semicolon list, `Core`, an empty list, or `All`.
The standalone full demo defaults to All. Embedded builds and standalone builds
with the demo disabled default to no new optional modules. Audio and Gallery
retain their original ON defaults; disable those explicitly when not used.
`CyUI::Core` never links them. The old Widgets.h/CyUI.h declarations remain source compatible.

Without CMake, compile the four Core sources and only the selected
`src/modules/<Name>.cpp` files plus their dependency closure. Define the matching
`CYUI_HAS_<NAME>` macros for Gallery/CyUI.h. CodeEditor also needs `src/modules/PagedCodeEditor.cpp`, TextEditor.cpp,
its include directory and C++20. Code and its dependents additionally compile
`src/modules/code/*.cpp`, the bundled `third_party/lexilla/lexlib/*.cxx` and
`lexers/*.cxx`, with Lexilla include/lexlib and Scintilla include directories.
Define `CYUI_CODE_LANGUAGE_CATALOGUE=1` for the extended C++ catalogue API. `build_demo.bat` is the complete Windows demo build.

## Shared data and edit contract {#shared-data-and-edit-contract}

- Give every object a stable, nonzero uint64 ID: results that return a bare ID
  (command palette, log/diagnostic rows, plot series, annotations) use 0 for none.
  Gestures track their targets with flags, so a 0 ID still drags and selects. Keep
  one state object per view.
- Wheel zoom and pan keep ranges usable: a step that would overflow a log axis or
  shrink a span below what its magnitude resolves (|value| x 1e-12) is ignored.
  Selection ranges are reported ordered (min <= max).
- ArrayView and callbacks borrow data for the call; keep it alive while drawing.
- Curve points are finite and sorted by time; timeline durations are positive.
  Units, snapping policy, graph types and interpolation belong to the host.
- Editors return `Edit<T>` proposals with `begin`, `update`, `commit`, `cancel`.
  Atomic actions may return commit directly. Preview state never mutates the host.
  Validate and apply a committed proposal as one Undo transaction. Increment the
  supplied revision when replacing host data during a gesture to cancel stale edits.
- Ordinary rendering and model preparation run on the host UI thread. LogBuffer
  append/clear/snapshot are synchronized and may be called from producer threads.
- Colors use palette roles; UI geometry uses dp/style. Gradient colors, plot
  values, textures and scene matrices are application data.
- No module reads projects, executes Git actions, starts a terminal process,
  decodes audio/video, computes FFTs or runs a language server. Clipboard actions
  are local UI requests. Web explicitly creates a browser only when selected.

Prepare documents/envelopes when data changes. Draw visible rows using cached
models, and supply visible/bounded datasets for graphs and timelines. Plots
accept at most 65,536 points per series by default and report oversized IDs in
`skippedSeries`; call reducePlot or query a host index before drawing. Heatmaps
bound the displayed cells to 256 per axis. Diff uses patience anchors and bounded
LCS (4 million cells by default), exposing `approximate()` when an unmatched
region is displayed as a full replacement. Both source sides are retained; CRLF and
LF lines compare equal and a notice row reports differing line endings.
Source/Markdown inputs are limited to 32 MiB; large files need host paging.
Logs default to 10,000 rows / 8 MiB. Waveforms consume prepared min/max envelopes.

## Python {#python}

```sh
python -m pip install -e ".[ui]"
```

`cyui` remains the existing theme/widget API. `cyui_modules` contains optional
`audio`, `canvas`, `plots`, `curves`, `nodes`, `timeline`, `audioviews`, `code`,
`diff`, `inspector`, `assets`, `images`, `viewport`, `commands`, `diagnostics`,
`documents` (`documentviews` alias), `web` and `cef`. Import models without loading
imgui-bundle; drawing imports it lazily. No NumPy, media decoder or 3D engine is required.

Python uses the same IDs, ranges, palette roles, edit phases, origin policy and
bounded diff algorithm. Its CodeEditor uses ImGui multiline editing with native
Undo and a live syntax preview. The richer C++ TextEditor backend has its own API;
Python does not pretend to expose its multiple cursors/completion hooks.
`cyui_modules.cef.CEFRuntime` loads the optional C ABI library/helper explicitly;
CEF binaries are not included in the Python package. See [WEB.md](doc:web).

```image
{"id": "modules-view", "src": "assets/assets.webp", "alt": "Content browser with folders and thumbnails", "controls": "minimal"}
```

## Paged documents {#paged-documents}

`CodePaging.h` belongs to Code (C++17); `PagedCodeEditor.h` belongs to CodeEditor
(C++20). [The provider guide](doc:paged-code) specifies bounded reads, UTF-16
positions, contextual syntax and host-owned transactions. The paged editor does
not inherit the classic lexer's 32 MiB snapshot limit or copy the whole file.
Per-page and input budgets still apply. This native C++ API has no Python or Web
binding yet. Source packaging includes it when CodeEditor is selected; Unreal
Editor compilation and platform input still require host validation.
