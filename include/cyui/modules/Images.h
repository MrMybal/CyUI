#pragma once
#include "cyui/modules/Canvas.h"
#include <optional>
#include <string>

namespace cyui {
struct ImageData {
    ImTextureID texture = 0;
    int width = 0, height = 0;
    ImVec2 uvMin{0, 0}, uvMax{1, 1};
};
enum class ImageTool { inspect, crop };
struct ImageState {
    CanvasState canvas;
    bool fitPending = true, cropping = false;
    Point start;
    Rect crop, before;
    std::optional<double> zoomRequest; // applied about the viewport center on the next draw
};
struct ImageResult {
    int pixelX = -1, pixelY = -1;
    bool clicked = false;
    Edit<Rect> crop;
};
CYUI_API ImageResult imageViewport(const char *id, ImageState &, const ImageData &, ImVec2 size,
                          ImageTool tool = ImageTool::inspect);
// Buttons issue requests on the same state used by imageViewport or imageCompare.
CYUI_API void imageViewToolbar(const char *id, ImageState &);

enum class ImageCompareMode { sideBySide, wipe, blink };
struct ImageAnnotation {
    ItemId id = 0;
    Rect region;                  // pixels in the first image's coordinate system
    int colorIndex = 0;
    std::string label;
};
struct ImageCompareState {
    ImageState view;
    ImageCompareMode mode = ImageCompareMode::wipe;
    float divider = .5f;
    bool draggingDivider = false;
    ImVec2 previousPaneSize{};
};
struct ImageCompareOptions {
    const char *firstLabel = "Before", *secondLabel = "After";
    ArrayView<ImageAnnotation> annotations;
    std::optional<ItemId> selected;
    double blinkSeconds = .6;
};
struct ImageCompareResult {
    std::optional<ItemId> selected;
    bool dividerChanged = false;
};
// Both sources share a pixel scale and a view transform. Their top-left pixels align at (0, 0).
CYUI_API ImageCompareResult imageCompare(const char *id, ImageCompareState &, const ImageData &first,
                                         const ImageData &second, ImVec2 size = {},
                                         const ImageCompareOptions & = {});
struct GradientStop {
    ItemId id = 0;
    double position = 0;
    ImVec4 color{0, 0, 0, 1};
};
struct GradientState {
    ItemId selected = 0, dragging = 0; // valid while hasSelection / active
    std::vector<GradientStop> before, preview;
    std::uint64_t revision = 0;
    bool hasSelection = false, active = false;
};
CYUI_API ImVec4 sampleGradient(ArrayView<GradientStop>, double position, bool linearLight = false);
CYUI_API Edit<std::vector<GradientStop>> gradientEditor(const char *id, GradientState &,
                                               const std::vector<GradientStop> &, ImVec2 size = {},
                                               bool linearLight = false, std::uint64_t revision = 0);
} // namespace cyui
