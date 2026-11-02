#include <cyui/Core.h>

#if defined(CYUI_TEST_COMPAT)
#include <cyui/CyUI.h>
#include <cyui/Widgets.h>
#elif defined(CYUI_TEST_AUDIO)
#include <cyui/modules/Audio.h>
#elif defined(CYUI_TEST_GALLERY)
#include <cyui/Gallery.h>
#endif

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {
// Taking the address forces the linker to resolve the owning object file.
bool (*volatile coreButton)(const char *, cyui::Variant, ImVec2) = cyui::button;
#if defined(CYUI_TEST_AUDIO) || (defined(CYUI_TEST_COMPAT) && defined(CYUI_HAS_AUDIO))
bool (*volatile audioKnob)(const char *, float *, float, float, float, const cyui::KnobStyle &) = cyui::knob;
#endif
#if defined(CYUI_TEST_GALLERY) || (defined(CYUI_TEST_COMPAT) && defined(CYUI_HAS_GALLERY))
void (*volatile gallery)(bool *, int) = cyui::showGallery;
#endif
#if defined(CYUI_TEST_COMPAT)
bool check(bool ok, const char *message) {
    if (!ok)
        std::fprintf(stderr, "FAIL: %s\n", message);
    return ok;
}
// One frame in a fixed window whose content starts at (20, 20).
template <class F> void frame(F draw, ImVec2 mouse = {-100, -100}, int down = -1, float wheel = 0) {
    auto &io = ImGui::GetIO();
    io.AddMousePosEvent(mouse.x, mouse.y);
    if (down >= 0)
        io.AddMouseButtonEvent(0, down != 0);
    if (wheel != 0)
        io.AddMouseWheelEvent(0, wheel);
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({1000, 800});
    ImGui::Begin("Test", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove);
    ImGui::SetCursorScreenPos({20, 20});
    draw();
    ImGui::Dummy({0, 0});
    ImGui::End();
    ImGui::Render();
}
// Regression checks for audited module behaviour; each block runs when its module is linked.
bool moduleRegressions() {
    using namespace cyui;
    auto &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1000, 800};
    io.DeltaTime = 1.f / 60;
    loadFonts();
    unsigned char *pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID(ImTextureID(1));
    bool ok = true;
#if defined(CYUI_HAS_NODES)
    {
        // 1 -> 2 <-> 3 -> 4: nodes in and after the cycle still advance by column.
        std::vector<Node> nodes{{1, {}, "A", {}, {{11, "o"}}},
                                {2, {}, "B", {{21, "i"}}, {{22, "o"}}},
                                {3, {}, "C", {{31, "i"}}, {{32, "o"}, {33, "o"}}},
                                {4, {}, "D", {{41, "i"}}, {}}};
        std::vector<NodeLink> links{{1, 11, 21}, {2, 22, 31}, {3, 32, 21}, {4, 33, 41}};
        const auto layout = arrangeNodes(nodes, links);
        ok &= check(layout.size() == 4 && layout[0].position.x < layout[1].position.x &&
                        layout[1].position.x < layout[2].position.x && layout[2].position.x < layout[3].position.x,
                    "arrangeNodes ranks nodes in and after a cycle");
    }
#endif
#if defined(CYUI_HAS_DIFF)
    {
        DiffDocument diff;
        diff.setTexts("a\r\nb\r\n", "a\nb\n");
        bool changed = false;
        for (const auto &r : diff.rows())
            changed = changed || r.kind == DiffKind::added || r.kind == DiffKind::removed;
        ok &= check(!changed && !diff.rows().empty() && diff.rows().back().kind == DiffKind::notice,
                    "CRLF vs LF compares equal with a line-ending notice");
        diff.setTexts("a\r\nb\n", "a\nb\r\n");
        ok &= check(!diff.rows().empty() && diff.rows().back().kind == DiffKind::notice,
                    "line endings differing on separate lines are reported");
    }
#endif
#if defined(CYUI_HAS_DOCUMENTVIEWS)
    {
        Document doc;
        ok &= check(!doc.setMarkdown(""), "an empty document is not reparsed");
        doc.setMarkdown("**kwargs and *ptr **bold** `x`");
        const auto &runs = doc.blocks().front().inlines;
        ok &= check(runs.size() == 5 && runs[0].text == "**kwargs " && runs[0].kind == DocumentInlineKind::text &&
                        runs[2].text == "*ptr " && runs[3].kind == DocumentInlineKind::strong &&
                        runs[3].text == "bold" && runs[4].kind == DocumentInlineKind::code && runs[4].text == "x",
                    "unmatched inline markers are shown verbatim");
    }
#endif
#if defined(CYUI_HAS_DIAGNOSTICS)
    {
        LogBuffer buffer(5);
        std::vector<LogEntry> rows, fresh;
        for (int i = 0; i < 3; ++i)
            buffer.append(LogLevel::info, "first " + std::to_string(i));
        auto revision = buffer.snapshot(rows, 0);
        for (int i = 0; i < 4; ++i)
            buffer.append(LogLevel::info, "second " + std::to_string(i));
        revision = buffer.snapshot(rows, revision);
        buffer.snapshot(fresh, 0);
        bool same = rows.size() == fresh.size() && rows.size() == 5;
        for (std::size_t i = 0; same && i < rows.size(); ++i)
            same = rows[i].id == fresh[i].id && rows[i].text == fresh[i].text;
        ok &= check(same, "incremental log snapshot matches a full copy");
        LogBuffer other(5); // a state moved to another buffer with overlapping IDs
        for (int i = 0; i < 7; ++i)
            other.append(LogLevel::info, "other " + std::to_string(i));
        other.snapshot(rows, revision);
        ok &= check(rows.size() == 5 && rows.back().text == "other 6", "a snapshot from another buffer is fresh");
    }
#endif
#if defined(CYUI_HAS_TIMELINE)
    {
        // Steps below the ULP of t used to hang the ruler loop.
        TimelineState state;
        state.view = {1.7e18, 1.7e18 + 512};
        frame([&] { timeline("Timeline", state, {}, {}, {}, {600, 200}); });
        ok &= check(state.view.valid(), "timeline ruler at nanosecond epochs");
    }
#endif
#if defined(CYUI_HAS_PLOTS)
    {
        PlotState state;
        state.x = {{1, 1e300}, true};
        state.y = {{-1e15, 1e15 + 1}, false};
        auto draw = [&] { plot("Plot", state, {}, {600, 300}); };
        frame(draw, {300, 150});
        for (int i = 0; i < 20; ++i) {
            io.AddKeyEvent(ImGuiMod_Ctrl, true);
            frame(draw, {300, 150}, -1, i < 10 ? -40.f : 40.f);
        }
        io.AddKeyEvent(ImGuiMod_Ctrl, false);
        frame(draw);
        ok &= check(state.x.range.valid() && state.x.range.min > 0 && state.y.range.valid(),
                    "plot zoom keeps finite, usable axes");
    }
#endif
#if defined(CYUI_HAS_AUDIOVIEWS)
    {
        PlotState state;
        state.x.range = {0, 4};
        frame([&] { stereoScope("Scope", state, {}, {300, 200}); });
        ok &= check(state.x.range.min == -2 && state.x.range.max == 2 && state.y.range.min == -2 &&
                        state.y.range.max == 2,
                    "stereo scope shares one symmetric range");
    }
#endif
#if defined(CYUI_HAS_AUDIO)
    {
        float gain = -70;
        auto draw = [&] { fader("Fader", &gain, -70, 6, {dp(30), 200}); };
        const ImVec2 at{20 + dp(15), 210};
        frame(draw, at);
        frame(draw, at, 1);
        for (int i = 1; i <= 4; ++i) // one pixel per frame; ImGui floors mouse positions
            frame(draw, {at.x, at.y - float(i)});
        frame(draw, {at.x, at.y - 4}, 0);
        ok &= check(gain > -70 && gain < -50, "slow fader drags leave the bottom");
        float flat = 0;
        auto tiny = [&] { fader("Tiny", &flat, 0, 1e-7f, {dp(30), 200}); };
        frame(tiny, at);
        frame(tiny, at, 1);
        frame(tiny, {at.x, at.y - 3});
        frame(tiny, {at.x, at.y - 3}, 0);
        ok &= check(std::isfinite(flat), "a fader with a tiny range stays finite");
        float value = std::nanf("");
        KnobStyle style;
        style.flags = knobLogarithmic;
        auto knobDraw = [&] { knob("Knob", &value, 20, 20000, 1000, style); };
        frame(knobDraw, {20 + dp(21), 20 + dp(15)});
        frame(knobDraw, {20 + dp(21), 20 + dp(15)}, -1, 1);
        ok &= check(std::isfinite(value), "a NaN knob value recovers");
    }
#endif
#if defined(CYUI_HAS_IMAGES) && defined(CYUI_HAS_VIEWPORT)
    {
        // The gizmo's centre handle takes the click: the crop underneath must not start.
        ImageState image;
        TransformState gizmo;
        Edit<Rect> crop;
        Edit<Transform2D> move;
        const Transform2D value{{200, 150}, {100, 60}, 0};
        auto draw = [&] {
            crop = imageViewport("Image", image, {0, 256, 256}, {600, 400}, ImageTool::crop).crop;
            move = transformGizmo("Gizmo", gizmo, value);
        };
        frame(draw, {200, 150});
        frame(draw, {200, 150}); // canvases yield to an overlay seen the frame before
        frame(draw, {200, 150}, 1);
        ok &= check(move.phase == EditPhase::begin && crop.phase == EditPhase::none && !image.cropping,
                    "gizmo handles block the canvas underneath");
        frame(draw, {210, 150});
        frame(draw, {210, 150}, 0);
        ok &= check(move.phase == EditPhase::commit && move.value.center.x == 210, "gizmo drag commits");
    }
#endif
    return ok;
}
#endif
} // namespace

int main() {
    ImGui::CreateContext();
    cyui::Config config;
    config.dpiScale = 1.5f;
    cyui::apply(config);
    bool ok = coreButton != nullptr && std::abs(cyui::dp(10.0f) - 15.0f) < 0.01f;
    cyui::setTheme("navy");
    ok = ok && cyui::currentThemeKey() == "navy";
#if defined(CYUI_TEST_AUDIO) || (defined(CYUI_TEST_COMPAT) && defined(CYUI_HAS_AUDIO))
    ok = ok && audioKnob != nullptr && cyui::meterPosition(-70.0f) == 0.0f &&
         cyui::meterPosition(6.0f) == 1.0f && cyui::meterPosition(-12.0f) < cyui::meterPosition(0.0f);
#endif
#if defined(CYUI_TEST_GALLERY) || (defined(CYUI_TEST_COMPAT) && defined(CYUI_HAS_GALLERY))
    ok = ok && gallery != nullptr;
#endif
#if defined(CYUI_TEST_COMPAT)
    ok = moduleRegressions() && ok;
#endif
    ImGui::DestroyContext();
    if (!ok)
        std::fputs("CyUI module integration failed\n", stderr);
    return ok ? 0 : 1;
}
