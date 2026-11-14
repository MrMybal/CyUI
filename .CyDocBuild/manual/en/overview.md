---
id: overview
title: CyUI manual
description: A common Dear ImGui foundation for C++ and Python desktop tools.
section: Start
order: 0
revision: 3
---

CyUI is a themed widget library on top of **Dear ImGui 1.92+**, with independently selectable C++ and Python interface modules. It serves developers of editors, creative tools and desktop utilities who need consistent controls, localization and specialized views. The host application keeps its renderer, data, engines and Undo history.

```image
{"id":"workspace","src":"assets/overview.webp","alt":"CyUI demo with a docked gallery, properties and tool panels","controls":"minimal"}
```

## Start here {#getting-started}

- [Integrate CyUI](doc:integration) with your existing ImGui target.
- [Choose modules](doc:modules) and follow [the shared edit contract](doc:modules#shared-data-and-edit-contract).
- Explore [Core widgets](doc:widgets), [module recipes](doc:module-recipes), [localization](doc:localization) and [quick filters](doc:filtering).
- Use [Python](doc:python), [Web/CEF](doc:web) or the optional [Unreal Editor connector](doc:unreal).
- Read [licensing](doc:licensing) before redistributing dependencies.

## Explore the demo {#demo}

Extract the entire portable Windows ZIP, then run `cyui_demo.exe`. English is the default; `Launch-French.cmd` selects French. The gallery has live theme, density and language choices. Samples use synthetic data and generated preview textures. Its mixer, transport and project panels illustrate UI; they are not an audio engine, project system or complete editor.

```sh
cyui_demo.exe --locale fr
cyui_demo.exe --theme violet --comfortable
cyui_demo.exe --gallery-only --tab 21
cyui_demo.exe --gallery-only --tab 22
cyui_demo.exe --gallery-only --tab 23
cyui_demo.exe --gallery-only --tab 7 --image-graph
```

The native gallery contains Text, Buttons, Controls, Containers, Canvas, Plots, Curves, Nodes, Timeline, Audio views, Audio analysis, Spectral displays, Code, Diff, Details, Assets, Images, Viewport, Commands, Diagnostics, Documents, Web, Layout and Filtering. The Web page needs a separately configured CEF backend for live browsing; the ordinary downloadable demo does not bundle CEF or Unreal.

## Platforms and status {#platforms}

The current version is **1.2.0**. The project does not declare an alpha/beta label; development is active.

| Component | Availability |
| --- | --- |
| Core and ordinary C++ modules | Portable sources for Windows/Linux/macOS; local MSVC build/render validation on Windows, cross-platform CI configured |
| Standalone demo | Windows x64, Win32/DirectX 11; no Linux/macOS demo binary |
| Python | Python 3.10+, imgui-bundle for drawing; model imports are renderer-independent |
| CEF adapter | Optional SDK/runtime; Windows validated locally, Linux/macOS deployment requires target validation |
| Unreal connector | Optional UE 5.5+ Editor C++; local UE 5.8.4 Windows validation, other targets require validation |

Core is C++17. CodeEditor and the tested CEF SDK integration require C++20. The Windows demo uses system fonts; portable hosts supply fonts covering their languages. See [integration](doc:integration#startup) and [localization limits](doc:localization#what-applications-translate).

## Read offline {#offline-help}

Open **Documentation/Documentation.html** directly from disk. Keep the complete Documentation folder together, including DocData. There is no server, CDN, account or Internet dependency. Optional reference links to external projects require a connection only when you choose to follow them.

F1 and **Help → Documentation** in the demo open this local manual in the current interface language. The Help menu also opens the module, filtering and localization chapters. In the Python gallery, use the Documentation button or F1 for the active module. [Help integration](doc:help) describes the reusable Python helper and native routes.

Use the sidebar and its nested topics, or Ctrl+K to search prose, code and documented API symbols. A language change keeps page and explicit section IDs. Images and galleries enlarge **inside the documentation window**; Escape closes the enlarged view. Keyboard focus exposes controls, touch controls remain available, and session navigation retains reading positions. Personal annotations/preferences depend on the browser’s local storage permissions.

[Explore the illustrated module tour](doc:visual-tour).
