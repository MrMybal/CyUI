#pragma once
#include "cyui/modules/Viewport.h"
#include <ImGuizmo.h>

namespace cyui {
struct Gizmo3DState {
    std::array<float, 16> before{}, preview{};
    bool active = false, blocked = false;
    std::uint64_t revision = 0;
};
struct Gizmo3DOptions {
    ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE mode = ImGuizmo::LOCAL;
    bool orthographic = false, enabled = true;
    std::uint64_t revision = 0;
    const float *snap = nullptr;
};
// Host supplies its ImGuizmo target. Matrices are column-major, matching Viewport.
Edit<std::array<float, 16>> gizmo3D(const char *id, Gizmo3DState &, const std::array<float, 16> &matrix,
                                    const std::array<float, 16> &view,
                                    const std::array<float, 16> &projection, const Rect &,
                                    const Gizmo3DOptions & = {});
} // namespace cyui
