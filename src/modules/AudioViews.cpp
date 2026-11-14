#include "cyui/modules/AudioViews.h"
#include "ModuleInternal.h"
#include <limits>
#include <cstdio>
#include <unordered_set>

namespace cyui {
std::vector<Peak> makePeaks(ArrayView<float> data, std::size_t columns) {
    if (!columns || !data.count)
        return {};
    columns = std::min(columns, data.count);
    std::vector<Peak> out(columns);
    for (std::size_t x = 0; x < columns; ++x) {
        const auto a = x * data.count / columns, b = (x + 1) * data.count / columns;
        bool have = false;
        for (auto i = a; i < b; ++i)
            if (std::isfinite(data[i])) {
                if (!have) {
                    out[x] = {data[i], data[i]};
                    have = true;
                } else {
                    out[x].minimum = std::min(out[x].minimum, data[i]);
                    out[x].maximum = std::max(out[x].maximum, data[i]);
                }
            }
        if (!have)
            out[x] = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    }
    return out;
}
WaveformResult waveform(const char *id, WaveformState &s, const WaveformData &data, double playhead,
                        ImVec2 size) {
    CanvasState canvas;
    CanvasOptions o;
    o.grid = o.navigation = false;
    o.claimWheel = true;
    CanvasScope scope(id, canvas, size, o);
    const auto f = scope.frame();
    const auto room = f.bounds.size();
    auto *d = ImGui::GetWindowDrawList();
    auto &io = ImGui::GetIO();
    WaveformResult result;
    if (!s.view.valid())
        s.view = data.coverage.valid() ? data.coverage : Range{0, 1};
    const double mouse = s.view.min + (io.MousePos.x - f.bounds.min.x) / room.x * s.view.span();
    auto x = [&](double t) { return f.bounds.min.x + float((t - s.view.min) / s.view.span()) * room.x; };
    if (f.wheel != 0) {
        if (io.KeyShift)
            s.view = detail::panView(s.view, -f.wheel * s.view.span() * .1);
        else
            s.view = detail::zoomView(s.view, mouse, std::exp(-f.wheel * .15));
    }
    if (f.hovered && ImGui::IsMouseClicked(2))
        s.panning = true;
    if (!ImGui::IsMouseDown(2))
        s.panning = false;
    if (s.panning)
        s.view = detail::panView(s.view, -io.MouseDelta.x / room.x * s.view.span());
    if (f.hovered && ImGui::IsMouseClicked(0)) {
        s.selection = {mouse, mouse};
        s.anchor = mouse;
        s.selecting = true;
        result.seek = true;
        result.time = mouse;
    }
    if (s.selecting) {
        s.selection = {std::min(s.anchor, mouse), std::max(s.anchor, mouse)};
        result.selectionChanged = true;
        if (!ImGui::IsMouseDown(0))
            s.selecting = false;
    }
    if (f.hovered && ImGui::IsMouseDoubleClicked(0)) {
        s.selection = {0, 0};
        s.selecting = false;
        result.selectionChanged = true;
    }
    if (data.coverage.valid())
        for (std::size_t channel = 0; channel < data.channels.size(); ++channel) {
            const auto peaks = data.channels[channel];
            if (!peaks.count)
                continue;
            const float top = f.bounds.min.y + room.y * float(channel) / data.channels.size(),
                        height = room.y / data.channels.size(), mid = top + height * .5f;
            d->AddLine({f.bounds.min.x, mid}, {f.bounds.max.x, mid}, palette().grid, dp(1));
            const int columns = std::clamp(int(room.x), 1, 8192);
            for (int pixel = 0; pixel < columns; ++pixel) {
                const double t = s.view.min + (double(pixel) + .5) / columns * s.view.span();
                if (!data.coverage.contains(t))
                    continue;
                const auto index = std::min(peaks.count - 1, std::size_t((t - data.coverage.min) /
                                                                         data.coverage.span() * peaks.count));
                const auto p = peaks[index];
                if (std::isfinite(p.minimum) && std::isfinite(p.maximum))
                    d->AddLine({f.bounds.min.x + room.x * pixel / columns,
                                mid - std::clamp(p.maximum, -1.f, 1.f) * height * .45f},
                               {f.bounds.min.x + room.x * pixel / columns,
                                mid - std::clamp(p.minimum, -1.f, 1.f) * height * .45f},
                               palette().info, dp(1));
            }
        }
    if (s.selection.min != s.selection.max)
        d->AddRectFilled({x(std::min(s.selection.min, s.selection.max)), f.bounds.min.y},
                         {x(std::max(s.selection.min, s.selection.max)), f.bounds.max.y},
                         palette().selection);
    d->AddLine({x(playhead), f.bounds.min.y}, {x(playhead), f.bounds.max.y}, palette().accent, dp(2));
    return result;
}
PlotResult spectrum(const char *id, PlotState &s, ArrayView<PlotSeries> data, ImVec2 size,
                    const PlotOptions &o) {
    s.x.logarithmic = true;
    if (!s.x.range.valid() || s.x.range.min <= 0)
        s.x.range = {20, 20000};
    return plot(id, s, data, size, o);
}
PlotResult spectrum(const char *id, PlotState &s, ArrayView<PlotSeries> data, ImVec2 size) {
    return spectrum(id, s, data, size, PlotOptions{});
}
PlotResult oscilloscope(const char *id, PlotState &s, ArrayView<PlotSeries> data, ImVec2 size,
                        const PlotOptions &o) {
    s.x.logarithmic = s.y.logarithmic = false;
    return plot(id, s, data, size, o);
}
PlotResult oscilloscope(const char *id, PlotState &s, ArrayView<PlotSeries> data, ImVec2 size) {
    return oscilloscope(id, s, data, size, PlotOptions{});
}
PlotResult stereoScope(const char *id, PlotState &s, ArrayView<PlotSeries> data, ImVec2 size,
                       const PlotOptions &o) {
    // Square, with one symmetric extent on both axes: wheel zoom scales it, the centre stays at 0.
    s.x.logarithmic = s.y.logarithmic = false;
    const auto avail = ImGui::GetContentRegionAvail();
    const float side = std::max(dp(1), std::min(size.x > 0 ? size.x : avail.x + size.x,
                                                 size.y > 0 ? size.y : avail.y + size.y));
    const double extent = s.x.range.valid() ? s.x.range.span() * .5 : 1;
    s.x.range = s.y.range = {-extent, extent};
    auto result = plot(id, s, data, {side, side}, o);
    const double zoomed = s.x.range.valid() ? s.x.range.span() * .5 : extent;
    s.x.range = s.y.range = {-zoomed, zoomed};
    return result;
}
PlotResult stereoScope(const char *id, PlotState &s, ArrayView<PlotSeries> data, ImVec2 size) {
    return stereoScope(id, s, data, size, PlotOptions{});
}
namespace {
std::string audioNumber(double value, const char *unit = "") {
    if (std::isnan(value) || value == HUGE_VAL) return "--";
    if (value == -HUGE_VAL) return std::string("-inf ") + unit;
    char text[80];
    std::snprintf(text, sizeof(text), "%.1f %s", value, unit);
    return text;
}
std::string audioHz(double value) {
    char text[48];
    std::snprintf(text, sizeof(text), value >= 1000 ? "%.2g kHz" : "%.3g Hz", value >= 1000 ? value / 1000 : value);
    return text;
}
void audioLabel(ImDrawList *d, ImVec2 at, const std::string &text, ImU32 color) {
    const auto extent = ImGui::CalcTextSize(text.c_str());
    d->AddRectFilled({at.x - dp(2), at.y}, {at.x + extent.x + dp(2), at.y + extent.y}, alpha(palette().bg0, .9f));
    d->AddText(at, color, text.c_str());
}
float audioFraction(double value, Range range) {
    return range.valid() && !std::isnan(value) ? float(std::clamp((value - range.min) / range.span(), 0., 1.)) : 0;
}
void audioBar(ImDrawList *d, Rect r, double value, Range range, ImU32 color, double marker = missingAudioValue) {
    d->AddRectFilled(r.min, r.max, palette().bg3, dp(2));
    const float x = r.min.x + r.size().x * audioFraction(value, range);
    if (!std::isnan(value)) d->AddRectFilled(r.min, {x, r.max.y}, color, dp(2));
    if (std::isfinite(marker) && range.contains(marker)) {
        const float m = r.min.x + r.size().x * audioFraction(marker, range);
        d->AddLine({m, r.min.y - dp(2)}, {m, r.max.y + dp(2)}, palette().text, dp(1));
    }
}
float audioHeight(ImVec2 size) {
    return std::max(dp(1), size.y > 0 ? size.y : ImGui::GetContentRegionAvail().y + size.y);
}
bool audioReset(bool enabled) {
    return enabled && ImGui::SmallButton(trLabel("Reset measurements###reset").c_str());
}
}

SpectrogramResult spectrogram(const char *id, PlotState &s, const SpectrogramData &data, ImVec2 size,
                              const SpectrogramOptions &o) {
    CanvasState canvas;
    CanvasOptions options;
    options.grid = options.navigation = false;
    options.claimWheel = o.navigation;
    CanvasScope scope(id, canvas, size, options);
    const auto &f = scope.frame();
    const auto r = f.bounds;
    const auto room = r.size();
    SpectrogramResult result;
    result.valid = data.frames && data.bins && data.frames <= data.values.count / data.bins &&
                   data.time.valid() && data.frequency.valid() &&
                   (!data.logarithmicBins || data.frequency.min > 0) && o.intensity.valid();
    if (!result.valid) return result;
    if (!s.x.range.valid()) s.x.range = data.time;
    if (!s.y.range.valid() || (s.y.logarithmic && s.y.range.min <= 0)) s.y.range = data.frequency;
    s.x.logarithmic = false;
    auto &io = ImGui::GetIO();
    auto mouse = [&] { return Point{s.x.value((io.MousePos.x - r.min.x) / room.x),
                                    s.y.value(1 - (io.MousePos.y - r.min.y) / room.y)}; };
    result.mouse = mouse();
    if (o.navigation) {
        if (f.hovered && ImGui::IsMouseClicked(2)) s.dragging = true;
        if (!ImGui::IsMouseDown(2)) s.dragging = false;
        if (s.dragging) {
            const bool x = detail::navigateAxis(s.x, -io.MouseDelta.x / room.x, result.mouse.x, 1);
            const bool y = detail::navigateAxis(s.y, io.MouseDelta.y / room.y, result.mouse.y, 1);
            result.viewChanged = x || y;
        }
        if (f.wheel != 0) {
            const double factor = std::exp(-f.wheel * .15);
            result.viewChanged = detail::navigateAxis(s.x, 0, result.mouse.x, factor) || result.viewChanged;
            if (io.KeyCtrl) result.viewChanged = detail::navigateAxis(s.y, 0, result.mouse.y, factor) || result.viewChanged;
        }
    } else s.dragging = false;
    result.mouse = mouse();
    const Axis time{data.time, false}, frequency{data.frequency, data.logarithmicBins};
    auto index = [](double fraction, std::size_t count) {
        return std::min(count - 1, std::size_t(std::clamp(fraction, 0., 1.) * count));
    };
    auto *d = ImGui::GetWindowDrawList();
    const auto budget = std::clamp<std::size_t>(o.maxCells, 1, 262144);
    // Sample screen-cell centres. Work is bounded even for a very large external matrix.
    const auto nx = std::max<std::size_t>(1, std::min({data.frames, budget, std::size_t(std::min(4096.f, std::ceil(room.x)))}));
    const auto ny = std::max<std::size_t>(1, std::min({data.bins, budget / nx, std::size_t(std::min(4096.f, std::ceil(room.y)))}));
    for (std::size_t y = 0; y < ny; ++y) {
        const double fy = frequency.fraction(s.y.value(1 - (y + .5) / ny));
        if (!std::isfinite(fy) || fy < 0 || fy > 1) continue;
        const auto bin = index(fy, data.bins);
        for (std::size_t x = 0; x < nx; ++x) {
            const double fx = time.fraction(s.x.value((x + .5) / nx));
            if (!std::isfinite(fx) || fx < 0 || fx > 1) continue;
            const float value = data.values[index(fx, data.frames) * data.bins + bin];
            if (!std::isfinite(value)) continue;
            const float t = audioFraction(value, o.intensity);
            const auto color = t < .5f ? mix(palette().bg0, palette().info, t * 2) :
                (t < .8f ? mix(palette().info, palette().accent, (t - .5f) / .3f) : mix(palette().accent, palette().warning, (t - .8f) / .2f));
            d->AddRectFilled({r.min.x + room.x * float(x) / nx, r.min.y + room.y * float(y) / ny},
                             {r.min.x + room.x * float(x + 1) / nx, r.min.y + room.y * float(y + 1) / ny}, color);
        }
    }
    if (o.axes) {
        for (int i = 0; i <= 4; ++i) {
            const float part = i / 4.f;
            audioLabel(d, {r.min.x + dp(4), std::max(r.min.y + dp(3), r.max.y - room.y * part - ImGui::GetFontSize() - dp(3))}, audioHz(s.y.value(part)), palette().textDim);
        }
        audioLabel(d, {r.min.x + dp(65), r.max.y - ImGui::GetFontSize() - dp(3)}, audioNumber(s.x.range.min, "s") + "  /  " + audioNumber(s.x.range.max, "s"), palette().textDim);
        audioLabel(d, {r.min.x + dp(65), r.min.y + dp(3)}, audioNumber(o.intensity.min, o.unit ? o.unit : "") + " / " + audioNumber(o.intensity.max, o.unit ? o.unit : ""), palette().textDim);
    }
    const double fx = time.fraction(result.mouse.x), fy = frequency.fraction(result.mouse.y);
    result.hovered = f.hovered && std::isfinite(fx) && std::isfinite(fy) && fx >= 0 && fx <= 1 && fy >= 0 && fy <= 1;
    if (result.hovered) {
        result.frame = index(fx, data.frames); result.bin = index(fy, data.bins);
        result.intensity = data.values[result.frame * data.bins + result.bin];
        result.clicked = ImGui::IsMouseClicked(0);
        d->AddLine({io.MousePos.x, r.min.y}, {io.MousePos.x, r.max.y}, alpha(palette().text, .4f), dp(1));
        d->AddLine({r.min.x, io.MousePos.y}, {r.max.x, io.MousePos.y}, alpha(palette().text, .4f), dp(1));
        ImGui::SetTooltip("%s | %s | %s", audioNumber(result.mouse.x, "s").c_str(), audioHz(result.mouse.y).c_str(), audioNumber(result.intensity, o.unit ? o.unit : "").c_str());
    }
    return result;
}

AudioMeterResult loudness(const char *id, PlotState &view, const LoudnessData &data, ImVec2 size, const LoudnessOptions &o) {
    ImGui::PushID(id); ImGui::BeginGroup();
    const float height = audioHeight(size);
    AudioMeterResult result; result.resetRequested = audioReset(o.resetButton);
    const float header = o.resetButton ? ImGui::GetFrameHeightWithSpacing() : 0;
    const double values[]{data.momentary, data.shortTerm, data.integrated,
        std::isfinite(data.rangeLow) && std::isfinite(data.rangeHigh) && data.rangeHigh >= data.rangeLow ? data.rangeHigh - data.rangeLow : missingAudioValue};
    const char *labels[]{"Momentary", "Short-term", "Integrated", "Loudness range"};
    const Range levels = o.levels.valid() ? o.levels : Range{-60, 0};
    {
        CanvasState canvas; CanvasOptions options; options.grid = options.navigation = false;
        CanvasScope scope("##readings", canvas, {size.x, dp(66)}, options);
        const auto r = scope.frame().bounds; auto *d = ImGui::GetWindowDrawList();
        for (int n = 0; n < 4; ++n) {
            const float left = r.min.x + r.size().x * n / 4 + dp(6), right = r.min.x + r.size().x * (n + 1) / 4 - dp(6);
            d->PushClipRect({left, r.min.y}, {std::max(left, right), r.max.y}, true);
            audioLabel(d, {left, r.min.y + dp(5)}, tr(labels[n]), palette().textDim);
            audioLabel(d, {left, r.min.y + dp(23)}, audioNumber(values[n], n == 3 ? "LU" : "LUFS"), palette().text);
            const Rect bar{{left, r.min.y + dp(48)}, {std::max(left, right), r.min.y + dp(57)}};
            if (n != 3) audioBar(d, bar, values[n], levels, palette().info, o.target);
            else {
                audioBar(d, bar, missingAudioValue, levels, palette().info);
                if (std::isfinite(values[n])) {
                    const float a = bar.min.x + bar.size().x * audioFraction(data.rangeLow, levels), b = bar.min.x + bar.size().x * audioFraction(data.rangeHigh, levels);
                    d->AddRectFilled({a, bar.min.y}, {b, bar.max.y}, palette().accent);
                }
            }
            d->PopClipRect();
        }
    }
    view.y.range = levels; view.y.logarithmic = view.x.logarithmic = false;
    PlotOptions options; options.formatX = [](double v) { return audioNumber(v, "s"); };
    options.formatY = [](double v) { return audioNumber(v, "LUFS"); };
    plot("##history", view, data.history, {size.x, std::max(dp(40), height - header - dp(66) - ImGui::GetStyle().ItemSpacing.y)}, options);
    ImGui::EndGroup(); ImGui::PopID(); return result;
}

AudioMeterResult truePeak(const char *id, ArrayView<TruePeakChannel> channels, ImVec2 size, const TruePeakOptions &o) {
    ImGui::PushID(id); ImGui::BeginGroup();
    const float height = audioHeight(size);
    AudioMeterResult result; result.resetRequested = audioReset(o.resetButton);
    for (std::size_t n = 0; n < channels.count; ++n)
        if ((!std::isnan(channels[n].level) && channels[n].level > o.ceiling) || channels[n].overs) ++result.clippedChannels;
    const float areaHeight = std::max(dp(24), height - (o.resetButton ? ImGui::GetFrameHeightWithSpacing() : 0));
    if (ImGui::BeginChild("##channels", {size.x, areaHeight})) {
        const auto range = o.levels.valid() ? o.levels : Range{-60, 6};
        const int count = int(std::min<std::size_t>(channels.count, std::numeric_limits<int>::max()));
        const float row = std::max(dp(38), ImGui::GetContentRegionAvail().y / std::max(1, count));
        ScopedVar tight(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0));
        ImGuiListClipper clipper; clipper.Begin(count, row);
        while (clipper.Step()) for (int n = clipper.DisplayStart; n < clipper.DisplayEnd; ++n) {
            ItemScope item(ItemId(n) + 1);
            CanvasState canvas; CanvasOptions options; options.grid = options.navigation = false;
            CanvasScope scope("##level", canvas, {0, row}, options);
            const auto r = scope.frame().bounds; auto *d = ImGui::GetWindowDrawList();
            const auto &v = channels[std::size_t(n)];
            const auto color = v.level > o.ceiling || v.overs ? palette().error : (v.level > o.warning ? palette().warning : palette().success);
            std::string text = std::string(v.label ? v.label : "") + "  " + audioNumber(v.level, "dBTP") + "   " + tr("Held") + " " + audioNumber(v.held, "dBTP") + "   " + tr("Overs") + " " + std::to_string(v.overs);
            audioLabel(d, {r.min.x + dp(6), r.min.y + dp(3)}, text, color);
            const Rect bar{{r.min.x + dp(6), r.min.y + dp(23)}, {std::max(r.min.x + dp(6), r.max.x - dp(6)), r.min.y + dp(32)}};
            audioBar(d, bar, v.level, range, color, o.ceiling);
            if (std::isfinite(v.held) && range.contains(v.held)) {
                const float x = bar.min.x + bar.size().x * audioFraction(v.held, range);
                d->AddLine({x, bar.min.y - dp(2)}, {x, bar.max.y + dp(2)}, palette().accent, dp(2));
            }
        }
        if (!channels.count) ImGui::TextDisabled("%s", tr("No channels").c_str());
    }
    ImGui::EndChild();
    ImGui::EndGroup(); ImGui::PopID(); return result;
}

PlotResult correlation(const char *id, PlotState &view, double value, ArrayView<PlotSeries> history, ImVec2 size) {
    ImGui::PushID(id); ImGui::BeginGroup(); const float height = audioHeight(size);
    {
        CanvasState canvas; CanvasOptions options; options.grid = options.navigation = false;
        CanvasScope scope("##gauge", canvas, {size.x, dp(48)}, options);
        const auto r = scope.frame().bounds; auto *d = ImGui::GetWindowDrawList();
        audioLabel(d, {r.min.x + dp(6), r.min.y + dp(3)}, tr("Correlation") + "  " + audioNumber(value), palette().text);
        const Rect bar{{r.min.x + dp(6), r.min.y + dp(26)}, {r.max.x - dp(6), r.min.y + dp(35)}};
        const float centre = (bar.min.x + bar.max.x) / 2;
        d->AddRectFilled(bar.min, {centre, bar.max.y}, alpha(palette().error, .3f));
        d->AddRectFilled({centre, bar.min.y}, bar.max, alpha(palette().success, .3f));
        d->AddLine({centre, bar.min.y}, {centre, bar.max.y}, palette().textDim, dp(1));
        if (std::isfinite(value)) {
            const float x = bar.min.x + bar.size().x * audioFraction(value, {-1, 1});
            d->AddCircleFilled({x, (bar.min.y + bar.max.y) / 2}, dp(5), value < 0 ? palette().error : palette().success);
        }
        audioLabel(d, {bar.min.x, bar.max.y}, "-1", palette().textDim);
        audioLabel(d, {bar.max.x - dp(16), bar.max.y}, "+1", palette().textDim);
    }
    PlotResult result;
    if (history.count) {
        view.y = {{-1, 1}, false}; view.x.logarithmic = false;
        PlotOptions options; options.formatX = [](double v) { return audioNumber(v, "s"); };
        result = plot("##history", view, history, {size.x, std::max(dp(40), height - dp(48) - ImGui::GetStyle().ItemSpacing.y)}, options);
    }
    ImGui::EndGroup(); ImGui::PopID(); return result;
}

ResponseResult filterResponse(const char *id, ResponseState &s, ArrayView<PlotSeries> gain,
    ArrayView<PlotSeries> phase, const std::vector<ResponsePoint> &points, ImVec2 size, const ResponseOptions &o) {
    ResponseResult result;
    const auto frequency = o.frequency.valid() && o.frequency.min > 0 ? o.frequency : Range{20, 20000};
    const auto gains = o.gain.valid() ? o.gain : Range{-24, 24};
    if (!s.view.x.range.valid() || s.view.x.range.min <= 0) s.view.x.range = frequency;
    if (!s.view.y.range.valid()) s.view.y.range = gains;
    s.view.x.logarithmic = true; s.view.y.logarithmic = false;
    auto options = o.plot;
    if (!options.formatX) options.formatX = audioHz;
    if (!options.formatY) options.formatY = [](double v) { return audioNumber(v, "dB"); };
    result.plot = plot(id, s.view, gain, size, options);
    const Rect r{ImGui::GetItemRectMin(), ImGui::GetItemRectMax()}; const auto room = r.size();
    auto *d = ImGui::GetWindowDrawList(); d->PushClipRect(r.min, r.max, true);
    const auto screen = [&](double hz, double db) { return ImVec2{r.min.x + float(s.view.x.fraction(hz)) * room.x, r.max.y - float(s.view.y.fraction(db)) * room.y}; };
    std::unordered_set<ItemId> ids;
    bool valid = points.size() <= 1024;
    if (valid) for (const auto &p : points) valid = p.id && std::isfinite(p.frequency) && p.frequency > 0 && std::isfinite(p.gain) && ids.insert(p.id).second && valid;
    bool same = points.size() == s.before.size();
    if (same) for (std::size_t n = 0; n < points.size(); ++n) {
        const auto &a = points[n], &b = s.before[n];
        same = same && a.id == b.id && a.frequency == b.frequency && a.gain == b.gain && a.enabled == b.enabled;
    }
    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (s.dragging && (o.readOnly || !valid || !same || o.revision != s.revision || !focused || ImGui::IsKeyPressed(ImGuiKey_Escape))) {
        result.points = {EditPhase::cancel, s.before}; s.dragging = false; s.active = 0;
    }
    if (o.phase.valid()) {
        const Axis axis{o.phase, false};
        for (std::size_t n = 0; n < phase.count; ++n) {
            const auto &line = phase[n];
            if (line.points.count > options.maxPoints) { result.plot.skippedSeries.push_back(line.id); continue; }
            bool previous = false; ImVec2 last{};
            for (std::size_t i = 0; i < line.points.count; ++i) {
                const auto p = line.points[i]; const double x = s.view.x.fraction(p.x), y = axis.fraction(p.y);
                if (!std::isfinite(x) || !std::isfinite(y)) { previous = false; continue; }
                const ImVec2 at{r.min.x + float(x) * room.x, r.max.y - float(y) * room.y};
                if (previous) d->AddLine(last, at, alpha(categorical(line.colorIndex), .65f), dp(1));
                previous = true; last = at;
            }
        }
        if (options.legend) {
            float x = r.min.x + dp(70);
            const auto phaseLabel = tr("Phase");
            for (std::size_t n = 0; n < phase.count; ++n) {
                const auto &line = phase[n];
                const auto text = phaseLabel + (line.label && line.label[0] && phaseLabel != line.label ? std::string(": ") + line.label : "");
                const float width = ImGui::CalcTextSize(text.c_str()).x + dp(18);
                if (x + width > r.max.x - dp(70)) break;
                audioLabel(d, {x, r.min.y + ImGui::GetFontSize() + dp(7)}, text, categorical(line.colorIndex));
                x += width;
            }
        }
        if (phase.count && options.axes) for (int i = 0; i <= 2; ++i) {
            const auto text = audioNumber(axis.value(i / 2.), "deg");
            audioLabel(d, {r.max.x - ImGui::CalcTextSize(text.c_str()).x - dp(4), std::clamp(r.max.y - room.y * i / 2, r.min.y + dp(3), std::max(r.min.y + dp(3), r.max.y - ImGui::GetFontSize() - dp(3)))}, text, palette().info);
        }
    }
    const auto &visible = s.dragging ? s.preview : points;
    ItemId hit = 0; float closest = dp(10); auto &io = ImGui::GetIO();
    if (valid) for (const auto &p : visible) {
        const auto at = screen(p.frequency, p.gain);
        if (!r.contains(at)) continue;
        d->AddCircleFilled(at, dp(5), p.enabled ? palette().accent : palette().bg5);
        d->AddCircle(at, dp(5), palette().text, 0, dp(1));
        const float distance = std::hypot(at.x - io.MousePos.x, at.y - io.MousePos.y);
        if (distance < closest) { closest = distance; hit = p.id; }
    }
    if (result.points.phase == EditPhase::none && !o.readOnly && !s.dragging && result.plot.hovered && hit && ImGui::IsMouseClicked(0)) {
        s.before = s.preview = points; s.active = hit; s.revision = o.revision; s.dragging = true;
        result.points = {EditPhase::begin, s.preview};
    } else if (s.dragging) {
        if (ImGui::IsMouseDown(0)) {
            for (auto &p : s.preview) if (p.id == s.active) {
                p.frequency = std::clamp(s.view.x.value(std::clamp(double(io.MousePos.x - r.min.x) / room.x, 0., 1.)), frequency.min, frequency.max);
                p.gain = std::clamp(s.view.y.value(std::clamp(1. - double(io.MousePos.y - r.min.y) / room.y, 0., 1.)), gains.min, gains.max);
            }
            result.points = {EditPhase::update, s.preview};
        } else { result.points = {EditPhase::commit, s.preview}; s.dragging = false; s.active = 0; }
    }
    if (hit && result.plot.hovered) for (const auto &p : visible) if (p.id == hit)
        ImGui::SetTooltip("%s | %s", audioHz(p.frequency).c_str(), audioNumber(p.gain, "dB").c_str());
    d->PopClipRect(); return result;
}

PlotResult dynamics(const char *id, PlotState &view, const DynamicsData &data, ImVec2 size, const DynamicsOptions &o) {
    ImGui::PushID(id); ImGui::BeginGroup(); const float height = audioHeight(size);
    {
        CanvasState canvas; CanvasOptions options; options.grid = options.navigation = false;
        CanvasScope scope("##reduction", canvas, {size.x, dp(43)}, options);
        const auto r = scope.frame().bounds; auto *d = ImGui::GetWindowDrawList();
        audioLabel(d, {r.min.x + dp(6), r.min.y + dp(3)}, tr("Gain reduction") + "  " + audioNumber(data.gainReduction, "dB"), palette().text);
        audioBar(d, {{r.min.x + dp(6), r.min.y + dp(26)}, {r.max.x - dp(6), r.min.y + dp(35)}}, data.gainReduction, o.reduction.valid() ? o.reduction : Range{0, 36}, palette().warning);
    }
    view.x.logarithmic = view.y.logarithmic = false;
    auto options = o.plot;
    if (!options.formatX) options.formatX = [](double v) { return audioNumber(v, tr("dB in").c_str()); };
    if (!options.formatY) options.formatY = [](double v) { return audioNumber(v, tr("dB out").c_str()); };
    auto result = plot("##transfer", view, data.transfer, {size.x, std::max(dp(40), height - dp(43) - ImGui::GetStyle().ItemSpacing.y)}, options);
    const Rect r{ImGui::GetItemRectMin(), ImGui::GetItemRectMax()}; const auto room = r.size(); auto *d = ImGui::GetWindowDrawList();
    d->PushClipRect(r.min, r.max, true);
    if (view.x.range.valid() && view.y.range.valid()) {
        const auto at = [&](double x, double y) { return ImVec2{r.min.x + float(view.x.fraction(x)) * room.x, r.max.y - float(view.y.fraction(y)) * room.y}; };
        const auto lo = std::max(view.x.range.min, view.y.range.min), hi = std::min(view.x.range.max, view.y.range.max);
        if (lo <= hi) d->AddLine(at(lo, lo), at(hi, hi), palette().lineStrong, dp(1));
        if (std::isfinite(data.input) && std::isfinite(data.output) && view.x.range.contains(data.input) && view.y.range.contains(data.output))
            d->AddCircleFilled(at(data.input, data.output), dp(5), palette().accent);
    }
    d->PopClipRect(); ImGui::EndGroup(); ImGui::PopID(); return result;
}
} // namespace cyui
