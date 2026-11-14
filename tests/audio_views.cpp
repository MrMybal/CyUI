#include <cyui/modules/AudioViews.h>
#include <imgui_internal.h>
#include <cstdio>
#include <limits>

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
