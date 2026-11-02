#include "cyui/modules/Canvas.h"

namespace cyui {
Point CanvasView::world(ImVec2 p, ImVec2 top) const {
    const double z = std::isfinite(zoom) && zoom > 0 ? zoom : 1;
    return {origin.x + (p.x - top.x) / z, origin.y + (p.y - top.y) / z};
}
ImVec2 CanvasView::screen(Point p, ImVec2 top) const {
    return {top.x + static_cast<float>((p.x - origin.x) * zoom),
            top.y + static_cast<float>((p.y - origin.y) * zoom)};
}
void CanvasView::zoomAt(ImVec2 p, double z, ImVec2 top) {
    const auto fixed = world(p, top);
    if (!std::isfinite(z) || z <= 0)
        return;
    zoom = z;
    origin = {fixed.x - (p.x - top.x) / zoom, fixed.y - (p.y - top.y) / zoom};
}
void CanvasView::fit(Point low, Point high, ImVec2 room, double padding) {
    if (!std::isfinite(low.x + low.y + high.x + high.y))
        return;
    zoom = std::clamp(std::min(std::max(1., room.x - padding * 2) / std::max(1., high.x - low.x),
                               std::max(1., room.y - padding * 2) / std::max(1., high.y - low.y)),
                      .001, 1000.);
    origin = {(low.x + high.x) * .5 - room.x / (2 * zoom), (low.y + high.y) * .5 - room.y / (2 * zoom)};
}
void drawCanvasGrid(const Rect &r, const CanvasView &v, double step) {
    if (step <= 0 || !std::isfinite(step) || !std::isfinite(v.zoom) || v.zoom <= 0)
        return;
    while (step * v.zoom < dp(12))
        step *= 2;
    const auto low = v.world(r.min, r.min), high = v.world(r.max, r.min);
    auto *d = ImGui::GetWindowDrawList();
    int budget = 4096;
    for (double x = std::ceil(low.x / step) * step; x <= high.x && budget-- > 0; x += step) {
        const auto p = v.screen({x, 0}, r.min);
        d->AddLine({p.x, r.min.y}, {p.x, r.max.y},
                   std::abs(x) < step * .01 ? palette().gridStrong : palette().grid, dp(1));
    }
    budget = 4096;
    for (double y = std::ceil(low.y / step) * step; y <= high.y && budget-- > 0; y += step) {
        const auto p = v.screen({0, y}, r.min);
        d->AddLine({r.min.x, p.y}, {r.max.x, p.y},
                   std::abs(y) < step * .01 ? palette().gridStrong : palette().grid, dp(1));
    }
}
CanvasScope::CanvasScope(const char *id, CanvasState &s, ImVec2 size, const CanvasOptions &o) {
    if (size.x <= 0)
        size.x = std::max(dp(1), ImGui::GetContentRegionAvail().x + size.x);
    if (size.y <= 0)
        size.y = std::max(dp(1), ImGui::GetContentRegionAvail().y + size.y);
    frame_.bounds.min = ImGui::GetCursorScreenPos();
    frame_.bounds.max = {frame_.bounds.min.x + size.x, frame_.bounds.min.y + size.y};
    ImGui::InvisibleButton(id, size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    frame_.hovered = ImGui::IsItemHovered();
    frame_.focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    auto &io = ImGui::GetIO();
    if (!std::isfinite(s.view.zoom) || s.view.zoom <= 0)
        s.view.zoom = 1;
    if (o.navigation) {
        if (frame_.hovered && (ImGui::IsMouseClicked(2) ||
                               (o.spacePan && ImGui::IsKeyDown(ImGuiKey_Space) && ImGui::IsMouseClicked(0))))
            s.panning = true;
        if (!ImGui::IsMouseDown(2) && !ImGui::IsMouseDown(0))
            s.panning = false;
        if (s.panning) {
            s.view.origin.x -= io.MouseDelta.x / s.view.zoom;
            s.view.origin.y -= io.MouseDelta.y / s.view.zoom;
        }
        if (frame_.hovered && io.MouseWheel != 0 && !wheelTaken()) {
            const double z =
                std::clamp(s.view.zoom * std::exp(io.MouseWheel * .15), double(o.minZoom), double(o.maxZoom));
            s.view.zoomAt(io.MousePos, z, frame_.bounds.min);
            takeWheel();
        }
    }
    frame_.mouse = s.view.world(io.MousePos, frame_.bounds.min);
    auto *d = ImGui::GetWindowDrawList();
    d->PushClipRect(frame_.bounds.min, frame_.bounds.max, true);
    d->AddRectFilled(frame_.bounds.min, frame_.bounds.max, palette().bg0);
    if (o.grid)
        drawCanvasGrid(frame_.bounds, s.view, o.gridStep);
}
CanvasScope::~CanvasScope() {
    ImGui::GetWindowDrawList()->PopClipRect();
}
} // namespace cyui
