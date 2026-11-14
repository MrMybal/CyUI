#include <cyui/modules/AudioViews.h>
#include <imgui_internal.h>
#include <cstdio>
#include <limits>
#include <numeric>

namespace {
int failures = 0;
void check(bool value, const char *message) {
    if (!value) { ++failures; std::fprintf(stderr, "FAIL: %s\n", message); }
}
template<class F> void frame(F draw, ImVec2 mouse = {-100, -100}, int button = -1,
                             bool down = false, bool escape = false, float wheel = 0) {
    auto &io = ImGui::GetIO(); io.AddMousePosEvent(mouse.x, mouse.y);
    if (button >= 0) io.AddMouseButtonEvent(button, down);
    if (escape) io.AddKeyEvent(ImGuiKey_Escape, true);
    if (wheel) io.AddMouseWheelEvent(0, wheel);
    ImGui::NewFrame(); ImGui::SetNextWindowPos({0, 0}); ImGui::SetNextWindowSize({1000, 800});
    ImGui::Begin("Audio tests", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetCursorScreenPos({20, 20}); draw(); ImGui::Dummy({0, 0});
    ImGui::End(); ImGui::Render();
    if (escape) io.AddKeyEvent(ImGuiKey_Escape, false);
}
}
int main() {
    using namespace cyui;
    ImGui::CreateContext(); auto &io = ImGui::GetIO(); io.IniFilename = nullptr;
    io.DisplaySize = {1000, 800}; io.DeltaTime = 1.f / 60; cyui::apply();
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    unsigned char *pixels; int w, h; io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h); io.Fonts->SetTexID(ImTextureID(1));

    std::vector<float> matrix; for (int n = 0; n < 16; ++n) matrix.push_back(float(n));
    SpectrogramData data{matrix, 4, 4, {0, 4}, {20, 20000}, true};
    PlotState view{{{0, 4}, false}, {{20, 20000}, true}};
    SpectrogramResult heat;
    auto drawHeat = [&] { heat = spectrogram("heat", view, data, {400, 200}); };
    frame(drawHeat); frame(drawHeat, {220, 120}); frame(drawHeat, {220, 120}, 0, true);
    check(heat.valid && heat.hovered && heat.clicked && heat.frame == 2 && heat.bin == 2 && heat.intensity == 10,
          "spectrogram hit maps time and log frequency to frame-major data");
    frame(drawHeat, {220, 120}, 0, false);
    frame(drawHeat, {220, 120}, -1, false, false, 1);
    check(heat.viewChanged && view.x.range.span() < 4 && std::abs((view.x.range.min + view.x.range.max) / 2 - 2) < 1e-6,
          "spectrogram zoom keeps pointer time anchored");
    data.frames = std::numeric_limits<std::size_t>::max();
    frame(drawHeat); check(!heat.valid, "malformed matrix dimensions cannot overflow validation");
    data.frames = 4; data.values = {}; frame(drawHeat); check(!heat.valid, "missing matrix is rejected");
    matrix.assign(1000000, -30); data.values = matrix; data.frames = data.bins = 1000;
    SpectrogramOptions budget; budget.maxCells = 8; budget.axes = false;
    frame([&] { spectrogram("bounded", view, data, {400, 200}, budget); });
    check(ImGui::GetDrawData()->TotalVtxCount < 200, "spectrogram cell budget bounds generated geometry");

    ColorRamp custom; const ImU32 stops[]{palette().info, palette().error}; custom.colors = {stops, 2};
    check(sampleRamp(custom, -1) == stops[0] && sampleRamp(custom, 2) == stops[1] && sampleRamp(custom, .5) == mix(stops[0], stops[1], .5f), "custom ramps interpolate palette colors and clamp endpoints");
    SpectrogramHistory history;
    check(history.reset(3, 2, .5, {20, 20000}, true), "stream history accepts a bounded uniform grid");
    for (int n = 0; n < 5; ++n) { const float column[]{float(n), float(n)}; check(history.append({column, 2}), "append analysis frame"); }
    auto snapshot = history.view();
    check(snapshot.frames == 3 && snapshot.values[0] == 2 && snapshot.values[5] == 4 && snapshot.time.min == 1 && snapshot.time.max == 2.5, "history wrap retains chronological frames and their exact coverage");
    check(!history.reset(10000000, 2, .5, {20, 20000}) && history.view().frames == 3 && !history.append({}), "invalid history operations preserve the previous snapshot");
    history.clear(10); const float column[]{-30, -40}; history.append({column, 2});
    check(history.view().time.min == 10 && history.view().time.max == 10.5, "clear establishes a new stream origin");
    matrix.resize(16); for (int n = 0; n < 16; ++n) matrix[n] = float(n);
    data = {matrix, 4, 4, {0, 4}, {20, 20000}, true};
    view = {{{0, 4}, false}, {{20, 20000}, true}};
    for (int orientation = 0; orientation < 2; ++orientation) for (bool reverse : {false, true}) {
        SpectrogramOptions options; options.orientation = orientation ? SpectrogramOrientation::timeVertical : SpectrogramOrientation::timeHorizontal; options.reverseTime = reverse;
        auto draw = [&] { heat = spectrogram("orientation", view, data, {400, 200}, options); };
        frame(draw); frame(draw, {120, 70});
        const auto expectedFrame = orientation ? (reverse ? 1u : 3u) : (reverse ? 3u : 1u);
        check(heat.hovered && heat.frame == expectedFrame && heat.bin == (orientation ? 1u : 3u), "both orientations and time directions preserve semantic hit coordinates");
    }
    SpectrogramOptions withLegend; withLegend.legend = true;
    frame([&] { heat = spectrogram("legend", view, data, {400, 200}, withLegend); }, {400, 80});
    check(!heat.hovered && heat.bounds.max.x < 400, "color legend is excluded from the data hit area");
    const float colorValues[]{-20, -20, -60, -60};
    SpectrogramData colorData{{colorValues, 4}, 2, 2, {0, 4}, {20, 20000}, true};
    SpectrogramOptions frequencyMode; frequencyMode.axes = false; frequencyMode.colorByFrequency = true;
    frequencyMode.ramp.style = RampStyle::frequency;
    ImU32 firstColor = 0, secondColor = 0;
    frame([&] {
        const int before = ImGui::GetWindowDrawList()->VtxBuffer.Size;
        spectrogram("frequency-mode", view, colorData, {400, 200}, frequencyMode);
        const auto &vertices = ImGui::GetWindowDrawList()->VtxBuffer;
        firstColor = vertices[before + 4].col; secondColor = vertices[before + 8].col;
    });
    check((firstColor & 0xffffffu) == (secondColor & 0xffffffu) && (firstColor >> 24) > (secondColor >> 24),
          "frequency grading keeps hue stable while signal level controls opacity");
    SpectrogramState following; following.window = 2;
    auto followDraw = [&] { heat = spectrogram("follow", following, data, {400, 200}); };
    frame(followDraw); frame(followDraw, {220, 120}); frame(followDraw, {220, 120}, -1, false, false, 1);
    check(!following.followLatest && following.view.x.range.span() < 2, "wheel navigation pauses automatic following");
    following.followLatest = true; frame(followDraw);
    check(following.view.x.range.min == 2 && following.view.x.range.max == 4, "resuming follow frames the latest requested time window");
    following.followLatest = false; io.DeltaTime = .4f; frame(followDraw); io.DeltaTime = 1.f / 60;
    frame(followDraw, {220, 120}); frame(followDraw, {220, 120}, 0, true);
    frame(followDraw, {220, 120}, 0, false); frame(followDraw, {220, 120}, 0, true);
    check(following.followLatest, "double-click resumes a manually paused stream");
    frame(followDraw, {220, 120}, 0, false);

    std::vector<Point> areaPoints{{20, -60}, {200, -20}};
    PlotSeries area{1, "", areaPoints, PlotStyle::area}; area.baseline = -90; area.gradient = true;
    PlotOptions plain; plain.axes = plain.legend = plain.grid = plain.crosshair = false;
    PlotState areaView{{{20, 20000}, true}, {{-90, 0}, false}};
    bool floorVertex = false;
    frame([&] {
        plot("area", areaView, {&area, 1}, {400, 200}, plain);
        for (const auto &v : ImGui::GetWindowDrawList()->VtxBuffer)
            floorVertex = floorVertex || (std::abs(v.pos.y - 220) < .1f && (v.col >> 24) == 0);
    });
    check(floorVertex, "gradient area reaches the explicit dB floor with a transparent baseline");
    SpectrumBand band{{20, 200}, -45}; BandSpectrumOptions bandOptions; bandOptions.plot = plain;
    frame([&] { bandSpectrum("bands", areaView, {&band, 1}, {}, {400, 200}, bandOptions); });
    bandOptions.plot.maxPoints = 0;
    PlotResult bandResult; frame([&] { bandResult = bandSpectrum("limited-bands", areaView, {&band, 1}, {}, {400, 200}, bandOptions); });
    check(bandResult.skippedSeries == std::vector<ItemId>{1}, "oversized audio bands report their series instead of allocating unbounded geometry");

    StereoDensityState density; StereoDensityOptions densityOptions;
    densityOptions.persistence.resolution = 32; densityOptions.persistence.diffusion = 0;
    densityOptions.persistence.axes = densityOptions.persistence.navigation = false;
    const Point mono[]{ {.6, .6}, {missingAudioValue, 1} };
    std::uint64_t densityRevision = 1;
    auto densityDraw = [&] { stereoDensity("density", density, {mono, 2}, densityRevision, {200, 200}, densityOptions); };
    frame(densityDraw); const float firstEnergy = std::accumulate(density.density.energy.begin(), density.density.energy.end(), 0.f);
    frame(densityDraw); const float fadedEnergy = std::accumulate(density.density.energy.begin(), density.density.energy.end(), 0.f);
    check(firstEnergy > 0 && fadedEnergy < firstEnergy, "unchanged trace revisions decay without being reinjected");
    ++densityRevision; frame(densityDraw);
    check(std::accumulate(density.density.energy.begin(), density.density.energy.end(), 0.f) > firstEnergy, "fresh trace revisions accumulate");
    densityOptions.persistence.halfLife = 0; frame(densityDraw); frame(densityDraw);
    check(std::accumulate(density.density.energy.begin(), density.density.energy.end(), 0.f) > 0, "disabling persistence keeps the current trace visible");
    SpatialScopeState space; SpatialScopeOptions spaceOptions; spaceOptions.persistence.resolution = 32;
    const SpatialSample samples[]{{1000, .6, -10}, {0, 0, 0}, {200, missingAudioValue, -10}};
    frame([&] { spatialScope("space", space, {samples, 3}, 1, {400, 200}, spaceOptions); });
    const auto maximum = std::max_element(space.density.energy.begin(), space.density.energy.end());
    check(maximum != space.density.energy.end() && *maximum > 0 && std::size_t(maximum - space.density.energy.begin()) % 32 >= 24,
          "spatial scope maps frequency and right-pan samples while ignoring invalid inputs");
    space.view.x.range = {-2, 2}; frame([&] { spatialScope("space", space, {}, 1, {400, 200}, spaceOptions); });
    check(std::accumulate(space.density.energy.begin(), space.density.energy.end(), 0.f) == 0, "changing the spatial view clears incompatible accumulated history");

    ResponseState state;
    std::vector<ResponsePoint> bands{{0x100000001ULL, std::sqrt(20. * 20000), 0}};
    ResponseOptions options;
    ResponseResult edit;
    auto response = [&] { edit = filterResponse("response", state, {}, {}, bands, {400, 200}, options); };
    frame(response); frame(response); frame(response, {220, 120}, 0, true);
    check(edit.points.phase == EditPhase::begin && state.active == bands[0].id, "EQ handles preserve 64-bit IDs");
    frame(response, {320, 80});
    check(edit.points.phase == EditPhase::update && edit.points.value[0].frequency > 3000 && edit.points.value[0].gain > 9 && bands[0].gain == 0,
          "EQ drag proposes logarithmic frequency and dB without mutating the model");
    frame(response, {320, 80}, -1, false, true);
    check(edit.points.phase == EditPhase::cancel && edit.points.value[0].gain == 0 && !state.dragging, "Escape cancels an EQ gesture");
    frame(response, {220, 120}, 0, false);
    frame(response, {220, 120});
    frame(response, {220, 120}, 0, true);
    check(edit.points.phase == EditPhase::begin, "second EQ gesture begins after cancellation");
    frame(response, {320, 80}); frame(response, {320, 80}, 0, false);
    check(edit.points.committed() && edit.points.value[0].gain > 9, "EQ gesture emits one commit on release");
    frame(response, {220, 120}, 0, true); ++options.revision; frame(response, {250, 100});
    check(edit.points.phase == EditPhase::cancel, "external revision invalidates an EQ preview");
    frame(response, {220, 120}, 0, false); frame(response, {220, 120}, 0, true);
    bands[0].gain = 3; frame(response, {250, 100});
    check(edit.points.phase == EditPhase::cancel, "changed model invalidates a preview even without a revision change");
    frame(response, {220, 120}, 0, false); bands[0].gain = 0; options.readOnly = true;
    frame(response, {220, 120}, 0, true);
    check(edit.points.phase == EditPhase::none && !state.dragging, "read-only response cannot begin editing");
    frame(response, {220, 120}, 0, false); options.readOnly = false;
    bands.push_back(bands[0]); frame(response, {220, 120}, 0, true);
    check(!state.dragging, "duplicate response IDs disable handle editing");
    frame(response, {220, 120}, 0, false);

    TruePeakChannel peaks[]{{"L", -1, .4, 3}, {"R", -HUGE_VAL, -3, 0}, {"Missing", missingAudioValue, missingAudioValue, 0}};
    AudioMeterResult meter;
    auto meters = [&] { meter = truePeak("peaks", {peaks, 3}, {600, 180}); };
    frame(meters); check(meter.clippedChannels == 1, "true peak reports overloads while accepting silence and unavailable channels");
    frame(meters, {35, 27}, 0, true); frame(meters, {35, 27}, 0, false);
    check(meter.resetRequested && peaks[0].overs == 3, "reset is a request; host measurements stay untouched");
    std::vector<TruePeakChannel> manyChannels(10000, {"Channel", -12, -6, 0});
    manyChannels.back().overs = 1;
    frame([&] { meter = truePeak("many", manyChannels, {600, 180}); });
    check(meter.clippedChannels == 1 && ImGui::GetDrawData()->TotalVtxCount < 5000,
          "channel list clips geometry while overload totals include offscreen channels");
    for (const auto &theme : builtInThemes()) {
        setTheme(theme.key.c_str());
        for (const auto *language : {"en", "fr"}) {
            setLocale(language);
            if (locale() == "fr") check(trLabel("Reset measurements###reset") == "Réinitialiser les mesures###reset", "French meter reset preserves a stable ID");
            frame([&] {
                PlotState history{{{0, 120}, false}, {{-60, 0}, false}};
                loudness("loudness", history, {}, {500, 150});
                correlation("correlation", history, missingAudioValue, {}, {500, 60});
                PlotState transfer{{{-60, 0}, false}, {{-60, 0}, false}};
                dynamics("dynamics", transfer, {}, {500, 150});
            });
        }
    }
    ImGui::DestroyContext(); return failures ? 1 : 0;
}
