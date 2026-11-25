#include <cyui/CyUI.h>
#include <cstdio>
#include <limits>

namespace {

bool check(bool condition, const char *message) {
    if (!condition) std::fprintf(stderr, "%s\n", message);
    return condition;
}

template<class Draw>
void frame(Draw draw) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({940, 780}, ImGuiCond_Always);
    ImGui::Begin("Widget tests", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetCursorScreenPos({20, 20});
    draw();
    ImGui::End();
    ImGui::Render();
}

#if defined(CYUI_HAS_IMAGES)
bool images(ImGuiIO &io) {
    cyui::ImageState state;
    state.fitPending = false;
    cyui::ImageData image{0, 500, 500};
    cyui::ImageResult result;
    const auto draw = [&](cyui::ImageTool tool = cyui::ImageTool::inspect) {
        frame([&] { result = cyui::imageViewport("image", state, image, {380, 240}, tool); });
    };
    io.AddMousePosEvent(80, 80);
    draw();
    io.AddMouseButtonEvent(0, true);
    draw();
    bool ok = check(!result.clicked, "inspect waits for mouse release before selecting");
    io.AddMousePosEvent(125, 105);
    draw();
    ok &= check(state.canvas.panning && state.canvas.view.origin.x == -45 && state.canvas.view.origin.y == -25,
                "left drag pans by the entire distance after the threshold");
    io.AddMouseButtonEvent(0, false);
    draw();
    ok &= check(!result.clicked && !state.canvas.panning, "ending a pan does not select a pixel");

    io.AddMouseButtonEvent(0, true);
    draw();
    io.AddMouseButtonEvent(0, false);
    draw();
    ok &= check(result.clicked && result.pixelX == 60 && result.pixelY == 60, "short click selects the pixel");

    io.AddMouseButtonEvent(0, true);
    draw(cyui::ImageTool::crop);
    ok &= check(result.crop.phase == cyui::EditPhase::begin, "crop sticks to the normal press gesture");
    io.AddMousePosEvent(179, 141);
    draw(cyui::ImageTool::crop);
    io.AddMouseButtonEvent(0, false);
    draw(cyui::ImageTool::crop);
    ok &= check(result.crop.committed() && result.crop.value.size().x > 0 && state.canvas.view.origin.x == -45,
                "crop drag makes a rectangle without panning");

    const auto center = state.canvas.view.world({210, 140}, {20, 20});
    state.zoomRequest = 2.;
    draw();
    const auto after = state.canvas.view.world({210, 140}, {20, 20});
    ok &= check(center.x == after.x && center.y == after.y && state.canvas.view.zoom == 2., "toolbar zoom anchors the center");

    cyui::ImageCompareState comparison;
    cyui::ImageCompareResult compare;
    ImVec2 top;
    const auto drawCompare = [&] {
        frame([&] {
            compare = cyui::imageCompare("compare", comparison, image, image, {400, 230});
            top = ImGui::GetItemRectMin();
        });
    };
    drawCompare();
    const auto origin = comparison.view.canvas.view.origin;
    io.AddMousePosEvent(top.x + 200, top.y + 95);
    drawCompare();
    io.AddMouseButtonEvent(0, true);
    drawCompare();
    io.AddMousePosEvent(top.x + 268, top.y + 95);
    drawCompare();
    ok &= check(compare.dividerChanged && comparison.divider > .65f &&
                    origin.x == comparison.view.canvas.view.origin.x && origin.y == comparison.view.canvas.view.origin.y,
                "dragging the wipe changes the divider without moving either source");
    io.AddMouseButtonEvent(0, false);
    drawCompare();
    return ok;
}
#endif

#if defined(CYUI_HAS_HEXVIEW)
bool hex() {
    using namespace cyui;
    const auto size = std::numeric_limits<std::uint64_t>::max();
    HexViewState state;
    state.anchor = state.reveal = size - 25;
    state.caret = size; // clamps to the last legal byte
    std::uint64_t offset = 0;
    std::size_t count = 0;
    int reads = 0;
    HexSource source{size, [&](std::uint64_t at, std::uint8_t *data, std::size_t n) {
        ++reads;
        offset = at;
        count = n;
        if (at >= size || n > size - at) return std::size_t{0};
        std::fill_n(data, n, std::uint8_t{0xab});
        return n > 0 ? n - 1 : 0; // exercise an incomplete page
    }};
    HexViewResult result;
    frame([&] { result = hexView("hex", state, source, {520, 240}); });
    bool ok = check(offset > size - 1024 && count < 1024 && reads == 1 && count <= size - offset,
                    "large virtual byte source reads only one visible window without overflow");
    ok &= check(result.selection && result.selection->offset == size - 25 && result.selection->length == 25,
                "byte selection clamps without losing the final byte");
    frame([&] { result = hexView("hex", state, {}, {520, 240}); });
    return ok && check(!result.selection && !state.anchor && state.topOffset == 0, "empty source clears interaction state");
}
#endif

#if defined(CYUI_HAS_TIMELINE)
bool timelines() {
    using namespace cyui;
    const std::vector<double> targets{4.75, 4.5, 4.25, std::numeric_limits<double>::quiet_NaN()};
    bool ok = check(snapTimelineTarget(4.51, targets, .25) == 4.5 &&
                     snapTimelineTarget(4.5, targets, 0) == 4.5 && !snapTimelineTarget(5, targets, .1),
                    "time snaps to the nearest finite in-range target including an exact match");
    Range view{-10, -5};
    TimelineNavigatorState state;
    frame([&] { timelineNavigator("overview", state, {0, 30}, view, {}, {540, 35}); });
    return ok && check(view.min == 0 && view.max == 5, "navigator bounds the visible window and preserves duration");
}
#endif

#if defined(CYUI_HAS_PLOTS)
bool plotDisplays(ImGuiIO &io) {
    using namespace cyui;
    const std::vector<float> density{.125f, .25f, .5f, 1.f};
    const std::vector<VideoScopePlane> planes{{density, 2, 2}, {density, 2, 2}, {density, 2, 2}};
    VideoScopeResult result;
    auto draw = [&] { frame([&] { result = videoScope("scope", planes, {400, 200}); }); };
    io.AddMousePosEvent(370, 175);
    draw();
    draw();
    bool ok = check(result.plane == 0 && result.column == 1 && result.row == 1 && result.density == 1,
                    "video scope maps pointer to the original density plane");
    VideoScopeOptions options;
    options.mode = VideoScopeMode::rgbParade;
    frame([&] { result = videoScope("scope", {}, {400, 200}, options); });
    ok &= check(result.plane == -1, "scope tolerates unavailable measurements");
    const std::vector<Point> points{{0, 0}, {1, 2}, {2, std::numeric_limits<double>::quiet_NaN()}, {3, 1}, {4, 3}};
    frame([&] { sparkline("trend", points, {400, 60}); });
    return ok && check(ImGui::GetDrawData()->TotalVtxCount > 0, "sparkline renders sparse samples");
}
#endif

#if defined(CYUI_HAS_NODES)
bool graph() {
    using namespace cyui;
    NodeState state;
    state.reveal = 0;
    const std::vector<Node> nodes{{0, {1200, 1000}, "Target", {}, {}, 0}};
    const std::vector<GraphChange> changes{{0, GraphChangeKind::modified, false, "Target", "Changed"}};
    NodeOptions options;
    options.readOnly = true;
    options.changes = changes;
    NodeResult result;
    frame([&] { result = nodeEditor("nodes", state, nodes, {}, {400, 240}, options); });
    const auto center = state.canvas.view.world({220, 140}, {20, 20});
    return check(result.revealed == 0 && !state.reveal && state.selection == std::vector<ItemId>{0} &&
                     center.x == 1200 + dp(options.width) * .5 && center.y == 1000 + dp(options.header + options.row + 8) * .5,
                 "read-only graph navigation reveals and centers node zero");
}
#endif

#if defined(CYUI_HAS_DIAGNOSTICS)
bool tasks() {
    using namespace cyui;
    LogBuffer log;
    log.append(LogLevel::info, "Progress update");
    const std::vector<TaskRowData> entries{{0, "Preview", "Drawing", TaskStatus::running, .5f, true, &log}};
    TaskPanelState state;
    state.selected = 0;
    TaskPanelResult result;
    frame([&] { result = taskPanel("tasks", state, entries, {400, 380}); });
    bool ok = check(result.selected == 0 && state.log.rows.size() == 1, "task zero has a log and stable selection");
    frame([&] { result = taskPanel("tasks", state, {}, {400, 380}); });
    return ok && check(result.selectionChanged && !result.selected && state.log.rows.empty(), "removed task drops its cached log");
}
#endif

#if defined(CYUI_HAS_CODE)
bool searches() {
    using namespace cyui;
    SearchResultsState state;
    state.selected = 0;
    int requests = 0;
    CodeDocument document;
    document.setText("line one\nline two");
    const std::vector<SearchHit> hits{{0, "test", "\xc3\xa9 needle", 2, 2, {{0, 3, 9, Tone::info, "needle"}}}};
    SearchResultsOptions options;
    options.preview = [&](const SearchHit &) { ++requests; return &document; };
    SearchResultsResult result;
    frame([&] { result = searchResults("search", state, hits, {460, 380}, options); });
    bool ok = check(result.selected == 0 && requests == 1 && state.previewed == 0, "search displays the host-provided preview for hit zero");
    AnnotatedTextState annotations;
    annotations.selected = 1;
    const std::vector<TextMark> marks{{1, 1, 2, Tone::warning, "Split UTF-8"}};
    frame([&] { annotatedText("text", annotations, "\xc3\xa9\nend", marks, {460, 220}, 1); });
    return ok && check(annotations.lineOffsets.size() == 2 && !annotations.selected, "annotations ignore misaligned UTF-8 boundaries");
}
#endif

} // namespace

int main() {
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1000, 820};
    io.DeltaTime = .4f; // independent clicks
    io.ConfigInputTrickleEventQueue = false;
    cyui::apply({});
    cyui::loadFonts();
    unsigned char *pixels = nullptr;
    int w = 0, h = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    io.Fonts->SetTexID(ImTextureID(1));
    bool ok = true;
#if defined(CYUI_HAS_IMAGES)
    ok &= images(io);
#endif
#if defined(CYUI_HAS_HEXVIEW)
    ok &= hex();
#endif
#if defined(CYUI_HAS_TIMELINE)
    ok &= timelines();
#endif
#if defined(CYUI_HAS_PLOTS)
    ok &= plotDisplays(io);
#endif
#if defined(CYUI_HAS_NODES)
    ok &= graph();
#endif
#if defined(CYUI_HAS_DIAGNOSTICS)
    ok &= tasks();
#endif
#if defined(CYUI_HAS_CODE)
    ok &= searches();
#endif
    ImGui::DestroyContext();
    return ok ? 0 : 1;
}
