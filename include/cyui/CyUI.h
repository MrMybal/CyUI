#pragma once

// CyUI - a better-looking, consistent Dear ImGui for the Cy* applications.
//
// Compatibility include for the enabled modules (link CyUI::CyUI):
//   #include <cyui/CyUI.h>
// For selective use, include Core.h and the desired module headers instead.
//
// Setup, once after ImGui::CreateContext() and before the first frame:
//   cyui::Config config;
//   config.themeKey = "mid-grey";            // see cyui::builtInThemes()
//   config.density = cyui::Density::compact;
//   config.dpiScale = dpi;                   // 1.0 at 96 dpi
//   cyui::apply(config);
//   cyui::loadFonts();
//
// Then use cyui:: widgets where stock ImGui looks plain (buttons, toggles,
// cards, properties, knobs, search...), and stock ImGui for the rest: the
// theme styles it too. See docs/AI_GUIDE.md for the rules.

#include "cyui/Core.h"
#include "cyui/Widgets.h"
#include "cyui/Gallery.h"

#define CYUI_VERSION "1.0.0"
#define CYUI_VERSION_NUM 10000
