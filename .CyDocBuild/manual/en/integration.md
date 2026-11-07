---
id: integration
title: Integrate CyUI
description: Integrate CyUI for CyUI integrators.
section: Start
order: 1
revision: 1
---

**Localization is required for interface text:** keep English as the default and
French as the second built-in language. Read [LOCALIZATION.md](doc:localization).
Use `cyui::trLabel("English label")` for controls/tabs/windows and
`cyui::tr("English text")` for display text. Register the application's messages
and provide a language choice; do not translate user content, paths or IDs.
Core includes localization automatically, without an extra module dependency.

**Requirements:** Dear ImGui **1.92 or newer** (the docking branch is
recommended) and C++17 (C++20 when CodeEditor or the optional CEF adapter is selected).

### CMake {#cmake}

```cmake
set(CYUI_IMGUI_TARGET <the app's imgui target>)   # e.g. cydaw_imgui
add_subdirectory(external/CyUI ${CMAKE_BINARY_DIR}/cyui)
target_link_libraries(<app> PRIVATE CyUI::CyUI)
```

`CyUI::CyUI` is the compatibility umbrella. For selective integration use
`<cyui/Core.h>` and `CyUI::Core`. Audio controls live in
`<cyui/modules/Audio.h>` / `CyUI::Audio`; the gallery and theme editor live in
`<cyui/Gallery.h>` / `CyUI::Gallery`. Audio and Gallery default to enabled;
disable `CYUI_BUILD_AUDIO` / `CYUI_BUILD_GALLERY` when not needed, and disable
`CYUI_BUILD_DEMO` for a selective build. Core never links those modules.
See [MODULES.md](doc:modules) for complete examples, the implemented optional targets and their exact dependency boundaries.

For example, a source-reading application selects only Code and Diff:

```cmake
set(CYUI_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_GALLERY OFF CACHE BOOL "" FORCE)
set(CYUI_MODULES "Code;Diff" CACHE STRING "" FORCE)
# add_subdirectory(...) after these options
target_link_libraries(app PRIVATE CyUI::Diff)
```

### Without CMake {#without-cmake}

Add `include/` to the include path. Compile `src/Theme.cpp`, `src/Widgets.cpp`,
`src/Layout.cpp`, `src/Filter.cpp` alongside the app's ImGui sources for Core. Add
`src/modules/Audio.cpp` for audio controls, and `src/Gallery.cpp` for the gallery.
Define `CYUI_HAS_AUDIO=1` when compiling Gallery with audio examples.

### Startup {#startup}

Do this after `ImGui::CreateContext()`, before the first `NewFrame`, and
**instead of** the app's old theme function (`ImGui::StyleColorsDark`,
`ApplyTheme`, a hand-written `style.Colors[...]` block):

```cpp
#include <cyui/CyUI.h>

cyui::Config config;
config.themeKey = "mid-grey";            // see the theme table below
config.density  = cyui::Density::compact; // or comfortable
config.dpiScale = dpiScale;              // 1.0 at 96 dpi
cyui::apply(config);
cyui::loadFonts();                       // Segoe UI + Semibold + Consolas + MDL2 icons
```

When the DPI changes (`WM_DPICHANGED`), call `cyui::applyDpi(newScale)`. It
does nothing when the scale is unchanged. `apply` and a real DPI change rebuild
the whole `ImGuiStyle`, so make your own style tweaks after them. At the end of
every frame, after everything else, call `cyui::renderNotifications()`. It
draws the toasts.

`Config` also has `squareWindows`: square, opaque windows for apps whose ImGui
windows are OS windows (multi-viewports, plug-in editors). `FontConfig` has
`size` (0: the density's font size), `fallbackFolder` (a folder of Inter,
Roboto, JetBrains Mono or Cousine `.ttf` files, tried when the Windows fonts
are missing) and `icons` (merge Segoe MDL2 Assets; on by default).

**Several ImGui contexts** (plug-in windows, editor panels): everything CyUI
keeps is per context and released when the context is destroyed. That covers
the palette, theme key, density, DPI, fonts, typed entry and toasts. Call
`apply` and `loadFonts` with each context current. Theme calls change the
current context only. With **no** context current, `setPalette`, `setTheme`
and `setDensity` change every live context, and the defaults new contexts
start from. Make such calls while no other thread draws with those contexts.
`apply` and `applyDpi` with no context current change the defaults only.

| themeKey      | look                                       |
|---------------|--------------------------------------------|
| `mid-grey`    | Mid greys, orange accent (default)          |
| `light-grey`  | Light greys, dark text, orange accent       |
| `amber-night` | Deep blue-greys, amber accent               |
| `violet`      | Dark slate, violet accent, cyan info        |
| `navy`        | Deep navy, blue accent, flat                |

Use `compact` density for dense pro tools (DAW, editors). Use `comfortable`
for launchers, hubs and settings-heavy apps.

---
