#pragma once
#include "cyui/modules/Canvas.h"
#include <string>
#include <functional>
#include <optional>

namespace cyui {
struct CYUI_API Axis {
    Range range;
    bool logarithmic = false;
    double fraction(double value) const;
    double value(double fraction) const;
};
enum class RampStyle { themed, intensity, frequency, monochrome };
struct ColorRamp {
    RampStyle style = RampStyle::themed;
    ArrayView<ImU32> colors; // optional 1..64 evenly spaced, caller-owned palette colors
};
CYUI_API ImU32 sampleRamp(const ColorRamp &, double fraction);
// Draw inside an existing clipped canvas. Vertical: low at bottom; horizontal: low at left.
CYUI_API void drawColorScale(const Rect &, const ColorRamp &, Range, const char *unit = "", bool vertical = true, bool logarithmic = false);
struct CurveFillOptions {
    ImU32 color = 0; // 0 selects palette().info; otherwise supply a palette-derived color
    float opacity = .18f;
    bool gradient = false; // fade to transparent at the baseline
};
// Screen-space fill in the current window/clip; reserves no item and draws no outline.
// X must increase within each run. Duplicate X/zero tails are harmless; non-finite
// points break runs and backwards segments are skipped. Baseline crossings are split.
// Strips have no internal AA fringes/overdraw. Draw the outline separately, once.
CYUI_API void drawCurveFill(ArrayView<ImVec2> points, float baseline, const CurveFillOptions & = {});
enum class PlotStyle { line, step, points, bars, area };
struct PlotSeries {
    ItemId id = 0;
    const char *label = "";
    ArrayView<Point> points;
    PlotStyle style = PlotStyle::line;
    int colorIndex = 0;
    double baseline = 0; // data units, also used by bars; audio commonly uses the dB floor
    float fillAlpha = 1;
    bool gradient = false, colorByX = false;
    ColorRamp ramp;
    ArrayView<Range> bands; // bars only: optional explicit left/right X edges, one per point
};
struct PlotState {
    Axis x, y;
    bool dragging = false;
};
struct PlotResult {
    bool hovered = false, viewChanged = false;
    Point mouse;
    ItemId series = 0; // nearest sample's series, 0 for none: give series nonzero IDs
    std::size_t index = 0;
    std::vector<ItemId> skippedSeries;
};
struct PlotOptions {
    bool grid = true, crosshair = true, navigation = true, axes = true, legend = true;
    std::function<std::string(double)> formatX, formatY;
    std::size_t maxPoints = 65536;
    // Called after navigation, inside canvas clipping, before grid/series/axes.
    std::function<void(const Rect &, const PlotState &)> drawBackground;
};
CYUI_API PlotResult plot(const char *id, PlotState &, ArrayView<PlotSeries>, ImVec2 size, const PlotOptions & = {});
// Preparation helpers: run when data changes, outside the draw loop. Gaps remain gaps.
CYUI_API std::vector<Point> reducePlot(ArrayView<Point>, std::size_t budget);
CYUI_API std::vector<std::uint64_t> histogram(ArrayView<double>, Range, std::size_t bins);
struct HeatmapResult {
    int row = -1, column = -1;
    bool clicked = false;
};
CYUI_API HeatmapResult heatmap(const char *id, ArrayView<float>, int columns, int rows, Range values, ImVec2 size);

struct SparklineOptions {
    std::optional<Range> y;
    int colorIndex = 0;
    bool fill = true;
};
// Compact plot of ordered samples. Non-finite values leave visible gaps.
CYUI_API PlotResult sparkline(const char *id, ArrayView<Point>, ImVec2 size = {},
                             const SparklineOptions & = {});

enum class VideoScopeMode { lumaWaveform, rgbParade, vectorscope };
struct VideoScopePlane {
    ArrayView<float> density; // row-major, row 0 at the top. Positive values are counts or intensities.
    int columns = 0, rows = 0;
};
struct VideoScopeOptions {
    VideoScopeMode mode = VideoScopeMode::lumaWaveform;
    float ceiling = 1;        // common reference intensity for all planes
    float gamma = .5f;
};
struct VideoScopeResult {
    int plane = -1, row = -1, column = -1;
    float density = 0;
};
// One plane for luma or chroma; three planes (R, G, B) for the parade.
CYUI_API VideoScopeResult videoScope(const char *id, ArrayView<VideoScopePlane>, ImVec2 size = {},
                                    const VideoScopeOptions & = {});
} // namespace cyui
