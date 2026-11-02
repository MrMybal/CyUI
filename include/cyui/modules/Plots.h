#pragma once
#include "cyui/modules/Canvas.h"
#include <string>
#include <functional>

namespace cyui {
struct Axis {
    Range range;
    bool logarithmic = false;
    double fraction(double value) const;
    double value(double fraction) const;
};
enum class PlotStyle { line, step, points, bars };
struct PlotSeries {
    ItemId id = 0;
    const char *label = "";
    ArrayView<Point> points;
    PlotStyle style = PlotStyle::line;
    int colorIndex = 0;
};
struct PlotState {
    Axis x, y;
    bool dragging = false;
};
struct PlotResult {
    bool hovered = false, viewChanged = false;
    Point mouse;
    ItemId series = 0;
    std::size_t index = 0;
    std::vector<ItemId> skippedSeries;
};
struct PlotOptions {
    bool grid = true, crosshair = true, navigation = true, axes = true, legend = true;
    std::function<std::string(double)> formatX, formatY;
    std::size_t maxPoints = 65536;
};
PlotResult plot(const char *id, PlotState &, ArrayView<PlotSeries>, ImVec2 size, const PlotOptions & = {});
// Preparation helpers: run when data changes, outside the draw loop. Gaps remain gaps.
std::vector<Point> reducePlot(ArrayView<Point>, std::size_t budget);
std::vector<std::uint64_t> histogram(ArrayView<double>, Range, std::size_t bins);
struct HeatmapResult {
    int row = -1, column = -1;
    bool clicked = false;
};
HeatmapResult heatmap(const char *id, ArrayView<float>, int columns, int rows, Range values, ImVec2 size);
} // namespace cyui
