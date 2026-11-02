#pragma once

// CyUI - Gallery: every widget on one window, a theme switcher, a palette
// editor. The CyUI equivalent of ImGui::ShowDemoWindow(): open it to see
// what exists and how it looks in each theme, and read Gallery.cpp for the
// code of each example.

#include "cyui/Export.h"
#include <imgui.h>

namespace cyui {
class WebBackend;
struct GalleryResources {
    ImTextureID texture = 0;
    int width = 0, height = 0;
    WebBackend *web = nullptr;
    bool previewCommands = false;
};

// selectTab >= 0 brings that page to the front (0 text, 1 buttons, 2 controls, 3 containers).
CYUI_API void showGallery(bool *open = nullptr, int selectTab = -1);
CYUI_API void showGallery(bool *open, int selectTab, const GalleryResources &);
// Live palette editor; saves / loads .cytheme files.
CYUI_API void showThemeEditor(bool *open = nullptr);

} // namespace cyui
