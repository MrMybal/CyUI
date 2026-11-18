#include <cyui/modules/AudioViews.h>
#include <imgui_internal.h>
#include <cstdio>
#include <limits>

namespace {
int failures = 0;
void check(bool condition, const char *message) {
    if (!condition) { ++failures; std::fprintf(stderr, "FAIL: %s\n", message); }
}
template<class F> void frame(F draw, ImVec2 mouse = {-100, -100}, int button = -1, bool down = false) {
    auto &io = ImGui::GetIO(); io.AddMousePosEvent(mouse.x, mouse.y);
    if (button >= 0) io.AddMouseButtonEvent(button, down);
    ImGui::NewFrame(); ImGui::SetNextWindowPos({0, 0}); ImGui::SetNextWindowSize({1000, 800});
    ImGui::Begin("Envelopes", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetCursorScreenPos({20, 20}); draw(); ImGui::Dummy({0, 0});
    ImGui::End(); ImGui::Render();
}
struct Triangle { ImVec2 a, b, c; };
double cross(ImVec2 a, ImVec2 b, ImVec2 p) {
    return (double(b.x) - a.x) * (double(p.y) - a.y) - (double(b.y) - a.y) * (double(p.x) - a.x);
}
bool inside(const Triangle &t, ImVec2 p) {
    if (std::abs(cross(t.a, t.b, t.c)) < 1e-7) return false;
    const double a = cross(t.a, t.b, p), b = cross(t.b, t.c, p), c = cross(t.c, t.a, p);
    return (a >= 0 && b >= 0 && c >= 0) || (a <= 0 && b <= 0 && c <= 0);
}
std::vector<Triangle> mesh(ImDrawList *draw, int firstIndex) {
    std::vector<Triangle> result;
    for (const auto &command : draw->CmdBuffer) {
        const auto end = command.IdxOffset + command.ElemCount;
        for (unsigned i = std::max(command.IdxOffset, unsigned(firstIndex)); i + 2 < end; i += 3) {
            const auto a = command.VtxOffset + draw->IdxBuffer[i];
            const auto b = command.VtxOffset + draw->IdxBuffer[i + 1];
            const auto c = command.VtxOffset + draw->IdxBuffer[i + 2];
            check(a < unsigned(draw->VtxBuffer.Size) && b < unsigned(draw->VtxBuffer.Size) &&
                  c < unsigned(draw->VtxBuffer.Size), "mesh indices include command vertex offsets");
            result.push_back({draw->VtxBuffer[a].pos, draw->VtxBuffer[b].pos, draw->VtxBuffer[c].pos});
        }
    }
    return result;
}
int coverage(const std::vector<Triangle> &triangles, ImVec2 p) {
    int count = 0; for (const auto &t : triangles) count += inside(t, p); return count;
}
}

int main() {
    using namespace cyui;
    ImGui::CreateContext(); auto &io = ImGui::GetIO(); io.IniFilename = nullptr;
    io.DisplaySize = {1000, 800}; io.DeltaTime = 1.f / 60; apply();
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    unsigned char *pixels; int width, height; io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID(ImTextureID(1));
    // Reproduce the native Prisme geometry: repeated peak, curved decay, zero tail,
    // repeated endpoint. Check actual triangles against the piecewise curve.
    for (float scale : {1.f, 1.5f}) for (float sustain : {0.f, .35f, 1.f}) for (int bend : {1, 3}) {
        std::vector<ImVec2> points{{20 * scale, 160 * scale}, {40 * scale, 40 * scale}, {40 * scale, 40 * scale}};
        for (int i = 1; i <= 32; ++i)
            points.push_back({(40 + 160 * i / 32.f) * scale,
                              float(160 - 120 * (sustain + (1 - sustain) * std::pow(1 - i / 32.f, bend))) * scale});
        points.push_back({250 * scale, (160 - 120 * sustain) * scale});
        points.push_back({300 * scale, 160 * scale}); points.push_back(points.back());
        std::vector<Triangle> triangles;
        frame([&] {
            auto *draw = ImGui::GetWindowDrawList(); const auto flags = draw->Flags;
            const int start = draw->IdxBuffer.Size, vertex = draw->VtxBuffer.Size;
            const CurveFillOptions options{palette().info, .23f, false};
            drawCurveFill(points, 160 * scale, options); triangles = mesh(draw, start);
            check(draw->Flags == flags, "fill preserves caller AA flags");
            for (int v = vertex; v < draw->VtxBuffer.Size; ++v)
                check(draw->VtxBuffer[v].col == alpha(palette().info, .23f), "fill has no transparent internal AA fringes");
        });
        int outside = 0, holes = 0, overlap = 0;
        for (float x = 21.37f * scale; x < 299 * scale; x += 3.17f * scale) {
            float line = 160 * scale;
            for (std::size_t i = 1; i < points.size(); ++i)
                if (points[i].x > points[i - 1].x && x >= points[i - 1].x && x < points[i].x) {
                    const float fraction = (x - points[i - 1].x) / (points[i].x - points[i - 1].x);
                    line = points[i - 1].y + fraction * (points[i].y - points[i - 1].y); break;
                }
            for (float y = 30.29f * scale; y < 170 * scale; y += 3.31f * scale) {
                if (std::abs(y - line) < .02f * scale) continue;
                const int count = coverage(triangles, {x, y});
                const bool expected = y >= line && y <= 160 * scale;
                outside += count && !expected; holes += !count && expected; overlap += count > 1;
            }
        }
        check(outside == 0 && holes == 0 && overlap == 0, "AHDSR fill follows curved decay without triangles above it, holes or overdraw");
    }
    std::vector<Triangle> triangles;
    const ImVec2 bipolar[]{{20, 40}, {180, 120}};
    frame([&] {
        auto *draw = ImGui::GetWindowDrawList(); const int start = draw->IdxBuffer.Size;
        drawCurveFill({bipolar, 2}, 80); triangles = mesh(draw, start);
    });
    check(coverage(triangles, {40, 65}) == 1 && coverage(triangles, {160, 95}) == 1 &&
          coverage(triangles, {40, 95}) == 0 && coverage(triangles, {160, 65}) == 0,
          "baseline crossing fills both sides without a self-intersecting quad");
    frame([&] {
        auto *draw = ImGui::GetWindowDrawList(); const int start = draw->VtxBuffer.Size;
        drawCurveFill({bipolar, 2}, 80, {palette().info, .4f, true});
        bool transparent = false, opaque = false;
        for (int v = start; v < draw->VtxBuffer.Size; ++v) {
            const auto &vertex = draw->VtxBuffer[v];
            if ((vertex.col & IM_COL32_A_MASK) == 0) { transparent = true; check(vertex.pos.y == 80, "gradient transparency stays on the baseline"); }
            else opaque = true;
        }
        check(transparent && opaque, "bipolar gradients retain curve opacity and fade at zero");
    });
    const ImVec2 gaps[]{{20, 40}, {70, 50}, {NAN, NAN}, {90, 40}, {130, 30}, {130, 30}, {110, 10}};
    frame([&] {
        auto *draw = ImGui::GetWindowDrawList(); const int start = draw->IdxBuffer.Size;
        drawCurveFill({gaps, 7}, 80); triangles = mesh(draw, start);
    });
    check(coverage(triangles, {40, 70}) == 1 && coverage(triangles, {80, 70}) == 0 &&
          coverage(triangles, {110, 70}) == 1, "NaN gaps, duplicate X and backwards edges cannot bridge/overdraw strips");
    frame([&] {
        auto *draw = ImGui::GetWindowDrawList(); const int before = draw->VtxBuffer.Size;
        drawCurveFill({}, 0); drawCurveFill({bipolar, 2}, NAN);
        drawCurveFill({bipolar, 2}, 80, {palette().info, NAN});
        drawCurveFill({bipolar, 2}, 80, {palette().info, 0});
        check(draw->VtxBuffer.Size == before, "invalid baseline/opacity and empty inputs emit no mesh");
    });
    std::vector<ImVec2> many;
    for (int i = 0; i < 20000; ++i) many.push_back({20 + i / 25.f, 40});
    frame([&] {
        auto *draw = ImGui::GetWindowDrawList(); const int start = draw->IdxBuffer.Size;
        drawCurveFill(many, 80); triangles = mesh(draw, start);
    });
    check(coverage(triangles, {400.017f, 60.13f}) == 1, "large fills survive the 16-bit draw-index boundary");

    std::vector<Point> samples{{0, 0}, {.1, 1}, {.1, 1}, {.5, 0}, {1, 0}, {1, 0}};
    EnvelopeStage stages[]{{{0, .1}, "Attack"}, {{.1, .1}, "Hold"}, {{.1, .5}, "Decay"},
                            {{.5, .8}, "Sustain"}, {{.8, 1}, "Release"}};
    EnvelopeData data{samples, {stages, 5}, .3, .5}; EnvelopeOptions options; options.timePower = .6;
    EnvelopeResult result;
    auto draw = [&] { result = envelope("envelope", data, {500, 230}, options); };
    frame(draw); check(result.valid, "AHDSR widget accepts zero-duration stages and repeated endpoints");
    const auto bounds = result.bounds;
    const ImVec2 mouse{bounds.min.x + float(std::pow(.3, .6)) * bounds.size().x,
                       bounds.max.y - bounds.size().y * .2f};
    frame(draw, mouse); frame(draw, mouse, 0, true);
    // ImGui floors incoming mouse coordinates to whole screen pixels.
    const double expectedTime = std::pow((io.MousePos.x - bounds.min.x) / bounds.size().x, 1 / options.timePower);
    const double expectedLevel = (bounds.max.y - io.MousePos.y) / bounds.size().y;
    check(result.hovered && result.clicked && result.stage == 2 && std::abs(result.mouse.x - expectedTime) < 1e-6 &&
          std::abs(result.mouse.y - expectedLevel) < 1e-6, "phase hit and mouse values invert the nonlinear display mapping");
    check(samples[2].x == .1 && samples[3].y == 0 && stages[1].time.min == stages[1].time.max,
          "drawing and clicking preserve host points/stages");
    frame(draw, mouse, 0, false);
    auto invalid = [&] {
        int vertices = 0;
        frame([&] { auto *list = ImGui::GetWindowDrawList(); const int before = list->VtxBuffer.Size;
                    draw(); vertices = list->VtxBuffer.Size - before; });
        check(!result.valid && vertices <= 4, "invalid/budget-exceeding envelope draws no partial curve or markers");
    };
    options.maxPoints = 2; invalid(); options.maxPoints = 65536;
    data.stages = {stages, 1000000}; invalid(); data.stages = {stages, 5};
    stages[1].time.min = .05; invalid(); stages[1].time.min = .1;
    samples[2].x = .05; invalid(); samples[2].x = .1;
    options.timePower = 0; invalid(); options.timePower = .6;
    options.time = {0, HUGE_VAL}; invalid(); options.time = {0, 1};
    options.levels = {1, 0}; invalid(); options.levels = {0, 1};
    options.phaseLabels = false; data.stages = {}; data.playhead = data.level = NAN;
    samples[2] = {NAN, NAN}; frame(draw);
    check(result.valid && result.bounds.min.y == 26, "custom envelopes support gaps, absent phases/markers and compact layout");
    frame([&] { result = envelope("tiny", data, {4, 4}); });
    check(!result.valid, "tiny envelope cannot generate inverted bounds");
    ImGui::DestroyContext();
    std::printf("Envelope rendering: %d failure(s)\n", failures);
    return failures ? 1 : 0;
}
