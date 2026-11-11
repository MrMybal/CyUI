#include <cmath>
#include <cstdio>
#include <cyui/Core.h>
#include <cyui/modules/Diff.h>
#include <imgui_internal.h>
#include <stdexcept>

namespace {
float rowHeight = 0;
void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
cyui::DiffResult frame(cyui::DiffState &state, const cyui::DiffDocument &document,
                       ImVec2 mouse = {-100, -100}, float wheel = 0, int button = -1,
                       bool down = false) {
    auto &io = ImGui::GetIO();
    io.AddMousePosEvent(mouse.x, mouse.y);
    if (wheel)
        io.AddMouseWheelEvent(0, wheel);
    if (button >= 0)
        io.AddMouseButtonEvent(button, down);
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({1100, 720});
    ImGui::Begin("Diff scroll fixture", nullptr,
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove);
    const auto result = cyui::diffViewer("diff", state, document, {0, 650});
    {
        cyui::ScopedFont font(cyui::monoFont());
        rowHeight = ImGui::GetTextLineHeightWithSpacing();
    }
    ImGui::End();
    ImGui::Render();
    require(ImGui::GetDrawData()->TotalVtxCount > 0, "Diff emitted no draw data");
    return result;
}
ImGuiWindow *canvas() {
    ImGuiWindow *found = nullptr;
    int count = 0;
    for (auto *window : ImGui::GetCurrentContext()->Windows)
        if (window->LastFrameActive == ImGui::GetFrameCount() &&
            (window->Flags & ImGuiWindowFlags_ChildWindow)) {
            found = window;
            ++count;
        }
    require(count == 1 && found, "Split comparison created independent scrolling children");
    return found;
}
void run() {
    auto &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1400, 1000};
    io.DeltaTime = 1.f / 60;
    cyui::apply();
    cyui::loadFonts();
    unsigned char *pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID(ImTextureID(1));
    std::string before, after;
    for (int i = 0; i < 600; ++i) {
        const auto line = "int line_" + std::to_string(i) + " = ";
        const std::string tail(200, 'x');
        before += line + "1; // " + tail + "\n";
        after += line + (i == 75 || i == 370 ? "2" : "1") + "; // " + tail + "\n";
    }
    cyui::DiffDocument doc;
    doc.setTexts(before, after, cyui::CodeLanguage::cpp);
    cyui::DiffState state;
    for (const auto *theme : {"mid-grey", "light-grey", "amber-night", "violet", "navy"})
        for (auto density : {cyui::Density::compact, cyui::Density::comfortable})
            for (float dpi : {1.f, 1.5f, 2.f})
                for (const auto *locale : {"en", "fr"}) {
                    cyui::Config config;
                    config.themeKey = theme;
                    config.density = density;
                    config.dpiScale = dpi;
                    cyui::apply(config);
                    cyui::setLocale(locale);
                    for (bool split : {true, false}) {
                        state.split = split;
                        frame(state, doc);
                        frame(state, doc);
                        require(canvas()->ScrollMax.y > 0,
                                "Diff did not expose its vertical scroll extent");
                    }
                }
    cyui::Config config;
    config.themeKey = "mid-grey";
    cyui::apply(config);
    state.split = true;
    state.scroll = 0;
    frame(state, doc);
    frame(state, doc);
    auto *child = canvas();
    require(child->ScrollMax.x > 0, "Long split lines lost horizontal scrolling");
    const auto left = ImVec2(child->Pos.x + child->Size.x * .25f, child->Pos.y + 120),
               right = ImVec2(child->Pos.x + child->Size.x * .75f, child->Pos.y + 120);
    frame(state, doc, left);
    frame(state, doc, left, -2);
    require(state.scroll > 0, "Wheel over the left column did not scroll the shared canvas");
    const auto old = state.scroll;
    frame(state, doc, right);
    frame(state, doc, right, -2);
    require(state.scroll > old, "Wheel over the right column did not scroll the same canvas");
    io.AddKeyEvent(ImGuiMod_Ctrl,true);frame(state,doc,right);frame(state,doc,right,1);
    require(state.textScale>1.f,"Ctrl+wheel in right diff column did not zoom the shared view");
    const auto zoom=state.textScale;state.wheelZoom=false;frame(state,doc,left,1);
    require(state.textScale==zoom,"Diff wheel-zoom opt-out failed");
    io.AddKeyEvent(ImGuiMod_Ctrl,false);state.wheelZoom=true;state.textScale=1.f;
    frame(state,doc);frame(state,doc);
    state.scroll = 500;
    frame(state, doc);
    require(std::abs(state.scroll - 500) < 1.f,
            "Assigning DiffState.scroll did not restore the requested shared offset");
    state.scroll = 0;
    frame(state, doc);
    frame(state, doc);
    const auto bar = ImGui::GetWindowScrollbarRect(child, ImGuiAxis_Y);
    const auto start = ImVec2(bar.GetCenter().x, bar.Min.y + 6);
    frame(state, doc, start, 0, 0, true);
    frame(state, doc, {start.x, bar.Max.y - 15}, 0, 0, true);
    frame(state, doc, {start.x, bar.Max.y - 15}, 0, 0, false);
    require(state.scroll > 1000,
            "Dragging the common vertical scrollbar did not move both columns");
    state.scroll = 0;
    frame(state, doc);
    ImGui::SetScrollX(child, 150);
    frame(state, doc);
    require(child->Scroll.x > 100, "Shared horizontal offset was not applied");
    const auto point =
        ImVec2(child->InnerRect.Min.x + 100,
               child->InnerRect.Min.y + ImGui::GetStyle().WindowPadding.y + rowHeight * 10.5f);
    frame(state, doc, point);
    const auto clicked = frame(state, doc, point, 0, 0, true);
    frame(state, doc, point, 0, 0, false);
    require(clicked.oldLine == 11 && clicked.newLine == 11,
            "Row height/clipping changed line navigation after horizontal scrolling");
    state.focus = doc.hunks().back();
    frame(state, doc);
    frame(state, doc);
    require(state.scroll > 0 && state.focus == -1,
            "Change navigation did not focus the shared canvas");
    state.split = false;
    state.scroll = 250;
    frame(state, doc);
    frame(state, doc);
    require(std::abs(state.scroll - 250) < 1.f, "Unified mode lost explicit scroll restoration");
    cyui::DiffDocument empty;
    empty.setTexts("", "");
    state = {};
    frame(state, empty);
    frame(state, empty);
    std::puts("CyUI Diff: single canvas, wheel on either side, scrollbar drag, horizontal gutters, "
              "row navigation, explicit scroll, focus, empty data and theme/locale/DPI matrix "
              "passed without OS windows.");
}
} // namespace
int main() {
    ImGui::CreateContext();
    try {
        run();
        ImGui::DestroyContext();
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        ImGui::DestroyContext();
        return 1;
    }
}
