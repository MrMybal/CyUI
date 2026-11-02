#pragma once
#include "cyui/Core.h"
#include "cyui/Interaction.h"

namespace cyui {
struct CanvasView {
    Point origin{};
    double zoom = 1;
    Point world(ImVec2 screen, ImVec2 topLeft = {}) const;
    ImVec2 screen(Point world, ImVec2 topLeft = {}) const;
    void zoomAt(ImVec2 anchor, double newZoom, ImVec2 topLeft = {});
    void fit(Point low, Point high, ImVec2 available, double padding = 24);
};
struct CanvasState {
    CanvasView view;
    bool panning = false;
};
struct CanvasOptions {
    bool grid = true, navigation = true, spacePan = true;
    double gridStep = 32;
    float minZoom = .05f, maxZoom = 64.f;
};
struct CanvasFrame {
    Rect bounds;
    bool hovered = false, focused = false;
    Point mouse;
};
// Reserves an ImGui item, handles navigation and clips all drawing until destruction.
class CanvasScope {
  public:
    CanvasScope(const char *id, CanvasState &, ImVec2 size, const CanvasOptions & = {});
    ~CanvasScope();
    CanvasScope(const CanvasScope &) = delete;
    CanvasScope &operator=(const CanvasScope &) = delete;
    const CanvasFrame &frame() const {
        return frame_;
    }

  private:
    CanvasFrame frame_;
};
void drawCanvasGrid(const Rect &, const CanvasView &, double step = 32);
} // namespace cyui
