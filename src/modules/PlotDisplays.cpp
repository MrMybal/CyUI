#include "cyui/modules/Plots.h"
#include "ModuleInternal.h"
#include <array>
#include <limits>

namespace cyui {

PlotResult sparkline(const char *id, ArrayView<Point> points, ImVec2 size, const SparklineOptions &options) {
    PlotState state;
    Range x{std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}, y = x;
    for (std::size_t index = 0; index < points.count; ++index) {
        const auto point = points[index];
        if (!std::isfinite(point.x)) continue;
        x = {std::min(x.min, point.x), std::max(x.max, point.x)};
        if (std::isfinite(point.y)) y = {std::min(y.min, point.y), std::max(y.max, point.y)};
    }
    const auto expanded = [](Range range) {
        if (range.valid() && std::isfinite(range.span())) return range;
        if (!std::isfinite(range.min) || range.min != range.max) return Range{0, 1};
        const auto pad = std::max(1., std::abs(range.min) * .01);
        const Range around{range.min - pad, range.max + pad};
        return around.valid() ? around : Range{0, 1};
    };
    state.x.range = expanded(x);
    state.y.range = options.y && options.y->valid() ? *options.y : expanded(y);
    PlotSeries series;
    series.id = 1;
    series.points = points;
    series.colorIndex = options.colorIndex;
    series.baseline = state.y.range.min;
    series.style = options.fill ? PlotStyle::area : PlotStyle::line;
    series.fillAlpha = .18f;
    PlotOptions plotOptions;
    plotOptions.grid = plotOptions.axes = plotOptions.navigation = plotOptions.legend = plotOptions.crosshair = false;
    plotOptions.maxPoints = 4096;
    if (size.y <= 0) size.y = dp(54);
    return plot(id, state, {&series, 1}, size, plotOptions);
}

namespace {
bool usable(const VideoScopePlane &plane) {
    return plane.columns > 0 && plane.rows > 0 && plane.columns <= 2048 && plane.rows <= 2048 &&
        plane.density.count >= std::size_t(plane.columns) * plane.rows;
}

void drawDensity(ImDrawList &draw, const VideoScopePlane &source, const Rect &box, ImU32 tint,
                 float ceiling, float gamma) {
    const auto size = box.size();
    const int columns = std::clamp(int(size.x / dp(3)), 1, std::min(72, source.columns));
    const int rows = std::clamp(int(size.y / dp(3)), 1, std::min(56, source.rows));
    const float dx = size.x / columns, dy = size.y / rows;
    for (int y = 0; y < rows; ++y) {
        const int firstY = y * source.rows / rows, lastY = (y + 1) * source.rows / rows;
        for (int x = 0; x < columns; ++x) {
            const int firstX = x * source.columns / columns, lastX = (x + 1) * source.columns / columns;
            float peak = 0;
            for (int sy = firstY; sy < lastY; ++sy) {
                for (int sx = firstX; sx < lastX; ++sx) {
                    const float value = source.density[std::size_t(sy) * source.columns + sx];
                    if (std::isfinite(value)) peak = std::max(peak, value);
                }
            }
            if (peak <= 0) continue;
            const float strength = std::pow(std::min(1.f, peak / ceiling), gamma);
            draw.AddRectFilled({box.min.x + x * dx, box.min.y + y * dy},
                               {box.min.x + (x + 1) * dx, box.min.y + (y + 1) * dy}, alpha(tint, strength));
        }
    }
}
} // namespace

VideoScopeResult videoScope(const char *id, ArrayView<VideoScopePlane> planes, ImVec2 size,
                            const VideoScopeOptions &options) {
    const bool vector = options.mode == VideoScopeMode::vectorscope;
    const bool parade = options.mode == VideoScopeMode::rgbParade;
    const int count = parade ? 3 : 1;
    const float ceiling = std::isfinite(options.ceiling) && options.ceiling > 0 ? options.ceiling : 1.f;
    const float gamma = std::isfinite(options.gamma) ? std::clamp(options.gamma, .1f, 4.f) : .5f;
    CanvasState state;
    CanvasOptions canvasOptions;
    canvasOptions.grid = canvasOptions.navigation = false;
    if (size.y <= 0) size.y = dp(220);
    CanvasScope canvas(id, state, size, canvasOptions);
    const auto &frame = canvas.frame();
    const auto &p = palette();
    auto &draw = *ImGui::GetWindowDrawList();
    const auto mouse = ImGui::GetIO().MousePos;
    const float pad = dp(8), axis = vector ? pad : dp(34);
    const float height = frame.bounds.size().y - pad * 2;
    const float available = frame.bounds.size().x - axis - pad * count;
    if (height <= 0 || available <= 0) return {};
    const float width = available / count;
    const std::array<ImU32, 3> channels{p.error, p.success, p.info};
    VideoScopeResult result;
    for (int index = 0; index < count; ++index) {
        const float left = frame.bounds.min.x + axis + index * (width + pad);
        Rect box{{left, frame.bounds.min.y + pad}, {left + width, frame.bounds.max.y - pad}};
        if (vector) {
            const auto square = std::min(width, height);
            box.min.x += (width - square) * .5f;
            box.min.y += (height - square) * .5f;
            box.max = {box.min.x + square, box.min.y + square};
        }
        draw.AddRectFilled(box.min, box.max, p.bg1);
        if (std::size_t(index) < planes.count && usable(planes[index])) {
            const auto &plane = planes[index];
            drawDensity(draw, plane, box, parade ? channels[index] : p.info, ceiling, gamma);
            if (frame.hovered && box.contains(mouse)) {
                result.plane = index;
                result.column = std::clamp(int((mouse.x - box.min.x) / box.size().x * plane.columns), 0, plane.columns - 1);
                result.row = std::clamp(int((mouse.y - box.min.y) / box.size().y * plane.rows), 0, plane.rows - 1);
                result.density = plane.density[std::size_t(result.row) * plane.columns + result.column];
            }
        }
        if (vector) {
            const float radius = box.size().x * .5f;
            const ImVec2 center{box.min.x + radius, box.min.y + radius};
            for (float ring : {.25f, .5f, .75f, 1.f}) draw.AddCircle(center, radius * ring, p.gridStrong);
            draw.AddLine({box.min.x, center.y}, {box.max.x, center.y}, p.gridStrong);
            draw.AddLine({center.x, box.min.y}, {center.x, box.max.y}, p.gridStrong);
        } else {
            constexpr const char *labels[]{"100", "75", "50", "25", "0"};
            for (int tick = 0; tick != 5; ++tick) {
                const float y = box.min.y + height * tick * .25f;
                draw.AddLine({box.min.x, y}, {box.max.x, y}, p.gridStrong);
                if (index == 0) draw.AddText({frame.bounds.min.x + dp(4),
                    std::clamp(y - ImGui::GetTextLineHeight() * .5f, frame.bounds.min.y,
                               std::max(frame.bounds.min.y, frame.bounds.max.y - ImGui::GetTextLineHeight()))}, p.textDim, labels[tick]);
            }
            if (parade) {
                constexpr const char *names[]{"R", "G", "B"};
                draw.AddText({box.min.x + pad, box.min.y + dp(3)}, p.text, names[index]);
            }
        }
        draw.AddRect(box.min, box.max, p.line);
    }
    return result;
}

} // namespace cyui
