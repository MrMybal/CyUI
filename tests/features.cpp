#include <cyui/CyUI.h>
#include <cstdio>
#include <limits>
#include <thread>
#include <stdexcept>
#include <random>
#if defined(CYUI_HAS_CODEEDITOR)
#include <TextEditor.h>
#endif

namespace {
int failures = 0;
void check(bool ok, const char *message) {
    if (!ok) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
}
bool near(double a, double b, double epsilon = 1e-6) {
    return std::abs(a - b) < epsilon;
}
template <class F>
void frame(F draw, float x = -100, float y = -100, int button = -1, bool down = false, bool escape = false) {
    auto &io = ImGui::GetIO();
    io.AddMousePosEvent(x, y);
    if (button >= 0)
        io.AddMouseButtonEvent(button, down);
    if (escape)
        io.AddKeyEvent(ImGuiKey_Escape, true);
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
    if (escape)
        io.AddKeyEvent(ImGuiKey_Escape, false);
}
} // namespace
int main() {
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1000, 800};
    io.DeltaTime = 1.f / 60;
    cyui::apply();
    cyui::loadFonts();
    unsigned char *pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID(ImTextureID(1));
    using namespace cyui;
    check(near(snapValue(1.24, .5), 1) && near(snapValue(-1.24, .5), -1), "snapping is symmetric");
    auto range = zoomRange({0, 10}, 3, .5);
    check(near(range.min, 1.5) && near(range.max, 6.5), "range zoom keeps anchor");
#if defined(CYUI_HAS_WEB)
    WebPolicy policy;
    policy.origins = {"http://127.0.0.1:4173", "https://EXAMPLE.com:443"};
    check(policy.allows("http://127.0.0.1:4173/chat") && policy.allows("https://example.com/path"),
          "web exact allowed origins");
    for (const char *url :
         {"file:///private", "javascript:alert(1)", "https://example.com.evil/", "https://example.com@evil/",
          "https://example.com\\@evil/", "http://127.0.0.1:4174", "https://example.com:999999999",
          "https://%65xample.com/"})
        check(!policy.allows(url), "web rejects untrusted URL");
    check(webOrigin("https://EXAMPLE.com:00443/a") == "https://example.com", "web canonical default port");
#endif
#if defined(CYUI_HAS_CANVAS)
    CanvasView canvas;
    canvas.origin = {1000000000, 2000000000};
    canvas.zoom = 2;
    auto fixed = canvas.world({100, 200});
    canvas.zoomAt({100, 200}, 4);
    const auto anchored = canvas.world({100, 200});
    check(near(fixed.x, anchored.x) && near(fixed.y, anchored.y), "canvas zoom anchor at large coordinates");
    const auto screen = canvas.screen(anchored);
    check(near(screen.x, 100) && near(screen.y, 200), "canvas coordinate inverse");
#endif
#if defined(CYUI_HAS_PLOTS)
    Axis logAxis{{20, 20000}, true};
    check(near(logAxis.value(logAxis.fraction(1000)), 1000), "log axis inverse");
    check(!std::isfinite(logAxis.fraction(0)), "log axis rejects zero");
    std::vector<Point> data;
    for (int i = 0; i < 1000; ++i)
        data.push_back({double(i), i == 501 ? 100. : 0.});
    auto reduced = reducePlot(data, 50);
    check(reduced.size() <= 50, "plot reduction budget");
    check(std::any_of(reduced.begin(), reduced.end(), [](Point p) { return p.y == 100; }),
          "plot reduction retains spike");
    data[300].y = std::numeric_limits<double>::quiet_NaN();
    reduced = reducePlot(data, 50);
    check(std::any_of(reduced.begin(), reduced.end(), [](Point p) { return !std::isfinite(p.y); }),
          "plot gaps retained");
    const std::vector<double> values{0, .5, 1, std::numeric_limits<double>::quiet_NaN(), -1};
    const auto bins = histogram(values, {0, 1}, 2);
    check(bins.size() == 2 && bins[0] == 1 && bins[1] == 2,
          "histogram endpoints, missing and out-of-range values");
#endif
#if defined(CYUI_HAS_CURVES)
    std::vector<CurvePoint> keys{{1, 0, 0}, {2, 1, 1}};
    check(near(evaluateCurve(keys, .5), .5), "linear curve");
    keys[0].shape = CurveShape::step;
    check(near(evaluateCurve(keys, .5), 0) && near(evaluateCurve(keys, 1), 1), "step curve boundary");
    keys[0].shape = CurveShape::bezier;
    keys[0].out = {.33, .33};
    keys[1].in = {-.33, -.33};
    check(near(evaluateCurve(keys, .5), .5), "Bezier inversion");
    CurveState curve;
    curve.view.x.range = {0, 10};
    curve.view.y.range = {0, 1};
    std::vector<CurvePoint> curvePoints{{1, 2, .2}};
    Edit<std::vector<CurvePoint>> edit;
    CurveOptions curveOptions;
    auto curves = [&] { edit = curveEditor("Curve", curve, curvePoints, {400, 200}, curveOptions); };
    frame(curves);
    frame(curves);
    frame(curves, 100, 180, 0, true);
    check(edit.phase == EditPhase::begin, "curve begins gesture");
    frame(curves, 140, 140);
    check(edit.phase == EditPhase::update && near(edit.value[0].time, 3) && near(edit.value[0].value, .4),
          "curve preview");
    frame(curves, 140, 140, 0, false);
    check(edit.committed() && near(curvePoints[0].time, 2), "curve proposal does not mutate host");
    io.DeltaTime = .4f;
    frame(curves);
    io.DeltaTime = 1.f / 60;
    frame(curves, 100, 180, 0, true);
    frame(curves, 140, 140);
    frame(curves, 140, 140, -1, false, true);
    check(edit.phase == EditPhase::cancel && !curve.dragging, "Escape cancels curve");
    frame(curves, 140, 140, 0, false);
#endif
#if defined(CYUI_HAS_NODES)
    std::vector<Node> nodes{{1, {0, 0}, "A", {}, {{11, "out", 0}}, 0},
                            {2, {300, 0}, "B", {{21, "in", 0}}, {}, 1}};
    std::vector<NodeLink> links{{1, 11, 21, 0}};
    const auto layout = arrangeNodes(nodes, links);
    check(layout[1].position.x > layout[0].position.x, "node arrangement follows links");
    NodeState nodeState;
    NodeResult nodeResult;
    auto nodeDraw = [&] { nodeResult = nodeEditor("Nodes", nodeState, nodes, {}, {700, 300}); };
    frame(nodeDraw);
    frame(nodeDraw);
    frame(nodeDraw, 210, 60, 0, true);
    frame(nodeDraw, 320, 60);
    frame(nodeDraw, 320, 60, 0, false);
    check(nodeResult.connectFrom == 11 && nodeResult.connectTo == 21, "typed pin connection proposal");
    auto linkDraw = [&] { nodeResult = nodeEditor("Nodes", nodeState, nodes, links, {700, 300}); };
    frame(linkDraw, 265, 60, 1, true);
    check(nodeResult.removeLink == 1, "node link removal proposal");
    frame(linkDraw, 265, 60, 1, false);
#endif
#if defined(CYUI_HAS_TIMELINE)
    TimelineState timelineState;
    std::vector<TimelineLane> lanes{{1, "Lane", 80}};
    std::vector<TimelineClip> clips{{1, 1, 1, 2, "Clip"}};
    TimelineResult timelineResult;
    TimelineOptions timelineOptions;
    timelineOptions.snap = .5;
    auto timelineDraw = [&] {
        timelineResult = timeline("Timeline", timelineState, lanes, clips, {}, {400, 200}, timelineOptions);
    };
    frame(timelineDraw);
    frame(timelineDraw);
    frame(timelineDraw, 195, 65, 0, true);
    frame(timelineDraw, 223, 65);
    frame(timelineDraw, 223, 65, 0, false);
    check(timelineResult.clips.committed() && near(timelineResult.clips.value[0].start, 2),
          "timeline move and snap");
#endif
#if defined(CYUI_HAS_AUDIOVIEWS)
    std::vector<float> audio{0, 1, -1, .5f};
    auto peaks = makePeaks(audio, 2);
    check(peaks.size() == 2 && peaks[0].maximum == 1 && peaks[1].minimum == -1,
          "waveform envelope preserves extrema");
#endif
#if defined(CYUI_HAS_IMAGES)
    std::vector<GradientStop> stops{{1, 0, {0, 0, 0, 1}}, {2, 1, {1, 1, 1, 1}}};
    const auto color = sampleGradient(stops, .5), linear = sampleGradient(stops, .5, true);
    check(near(color.x, .5) && linear.x > .7f, "gradient color-space policy");
    ImageState imageState;
    imageState.crop = {{10, 20}, {50, 60}};
    ImageResult imageResult;
    auto imageDraw = [&] {
        imageResult = imageViewport("Image", imageState, {0, 256, 256}, {400, 300}, ImageTool::crop);
    };
    frame(imageDraw);
    frame(imageDraw);
    frame(imageDraw, 200, 140, 0, true);
    check(imageResult.crop.phase == EditPhase::begin, "crop begins gesture");
    frame(imageDraw, 260, 200);
    frame(imageDraw, 260, 200, -1, false, true);
    check(imageResult.crop.phase == EditPhase::cancel && near(imageState.crop.min.x, 10) &&
              near(imageState.crop.max.y, 60),
          "crop Escape restores original selection");
    frame(imageDraw, 260, 200, 0, false);
#endif
#if defined(CYUI_HAS_VIEWPORT)
    OrbitCamera camera;
    camera.pitch = 0;
    camera.distance = 5;
    const auto eye = cameraPosition(camera);
    check(near(eye.x, 0) && near(eye.y, 0) && near(eye.z, 5), "camera convention");
    const auto matrix = cameraViewMatrix(camera);
    check(near(matrix[14], -5) && near(matrix[15], 1), "view matrix convention");
    camera.up = {0, 0, 0};
    check(near(cameraViewMatrix(camera)[0], 1), "zero up vector uses a valid basis");
#if defined(CYUI_HAS_IMGUIZMO)
    {
        Gizmo3DState state;
        const std::array<float, 16> transform{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        const std::array<float, 16> projection{1.5f, 0, 0,       0,  0, 2, 0,       0,
                                               0,    0, -1.002f, -1, 0, 0, -.2002f, 0};
        frame([&] {
            const auto result =
                gizmo3D("Gizmo", state, transform, matrix, projection, {{20, 20}, {520, 420}});
            check(result.phase == EditPhase::none && state.preview == transform,
                  "3D overlay preserves host matrix at rest");
        });
    }
#endif
#endif
#if defined(CYUI_HAS_COMMANDS)
    std::vector<Command> commands{{1, "Save document", "Ctrl+S", "File"},
                                  {2, "Save all", "Ctrl+Shift+S", "File"},
                                  {3, "Open document", "Ctrl+O", "File"}};
    const auto ranked = rankCommands(commands, "document save");
    check(ranked.size() == 1 && commands[ranked[0]].id == 1, "command search requires all terms");
    CommandState commandState;
    bool commandOpen = true;
    frame([&] { commandPalette("Command lifecycle", commandOpen, commandState, commands); });
    commandOpen = false;
    frame([&] {
        commandPalette("Command lifecycle", commandOpen, commandState, commands);
        check(!ImGui::IsPopupOpen("Command lifecycle"), "host can close the command modal");
    });
#endif
#if defined(CYUI_HAS_DIAGNOSTICS)
    LogBuffer logBuffer(3, 1024);
    std::thread writer([&] {
        for (int i = 0; i < 10; ++i)
            logBuffer.append(LogLevel::info, std::to_string(i));
    });
    writer.join();
    std::vector<LogEntry> log;
    const auto rev = logBuffer.snapshot(log, 0);
    check(log.size() == 3 && log.front().text == "7" && log.back().text == "9", "bounded thread-safe logs");
    check(logBuffer.snapshot(log, rev) == rev, "unchanged log snapshot");
#endif
#if defined(CYUI_HAS_DIAGNOSTICS)
    {
        LogState state;
        frame([&] { logView("Log", state, logBuffer, {500, 200}); });
        frame([&] { logView("Log", state, logBuffer, {500, 200}); });
        Diagnostic row{1, LogLevel::error, "source ## literal", "Example diagnostic", 12, 4};
        frame([&] { diagnosticList("Diagnostic", {&row, 1}, {500, 200}); });
    }
#endif
#if defined(CYUI_HAS_CODE)
    CodeDocument source;
    source.setText("\tfloat value = 42; // 音楽\r\n/* comment\ncontinued */\n", CodeLanguage::cpp, 7);
    check(source.lines().size() == 4 && source.lines()[0].text.rfind("    float", 0) == 0,
          "UTF-8, CRLF and tabs");
    check(source.lines()[2].spans.front().role == TokenRole::comment, "multiline lexer state");
    SourceSpan semantic{0, 1, 6, TokenRole::type};
    check(!source.setOverlays({&semantic, 1}, 6) && source.setOverlays({&semantic, 1}, 7),
          "semantic overlays use document revision");
    check(source.text().find('\t') == 0, "viewer preserves source bytes");
    check(source.text().find("\r\n") != std::string::npos, "viewer retains CRLF bytes");
    CodeViewState sourceState;
    frame([&] { codeViewer("Code", sourceState, source, {500, 300}); });
#endif
#if defined(CYUI_HAS_DIFF)
    DiffDocument diff;
    diff.setTexts("a\nb\nc\n", "a\nβ\nc\n");
    std::string oldText, newText;
    for (const auto &r : diff.rows()) {
        if (r.oldLine)
            oldText += r.code.text + '\n';
        if (r.newLine)
            newText += r.code.text + '\n';
    }
    check(oldText == "a\nb\nc\n" && newText == "a\nβ\nc\n", "diff retains both documents");
    check(diff.pairs().size() == 3 && !diff.hunks().empty(), "split diff alignment");
    diff.setUnified("--- a\n+++ b\n@@ -4,1 +9,1 @@\n-old\n+new\n");
    check(diff.rows()[3].oldLine == 4 && diff.rows()[4].newLine == 9, "unified hunk line numbers");
    DiffState diffState;
    frame([&] { diffViewer("Diff", diffState, diff, {600, 300}); });
    diff.setTexts("same", "same\n");
    check(!diff.hunks().empty() && diff.rows().back().kind == DiffKind::notice,
          "diff detects final newline-only change");
    diff.setUnified("@@ -999999999999999,1 +1,1 @@\n-untrusted\n");
    check(diff.rows().front().kind == DiffKind::notice, "oversized unified line numbers are handled");
    std::mt19937 random(529);
    for (int trial = 0; trial < 200; ++trial) {
        std::string a, b;
        for (unsigned n = random() % 25; n; --n) {
            a += char('a' + random() % 4);
            a += '\n';
        }
        for (unsigned n = random() % 25; n; --n) {
            b += char('a' + random() % 4);
            b += '\n';
        }
        diff.setTexts(a, b, CodeLanguage::plain, trial % 2 ? 0 : 4000000);
        std::string reconstructedA, reconstructedB;
        for (const auto &r : diff.rows()) {
            if (r.oldLine)
                reconstructedA += r.code.text + '\n';
            if (r.newLine)
                reconstructedB += r.code.text + '\n';
        }
        check(a == reconstructedA && b == reconstructedB, "bounded diff reconstruction property");
    }
#endif
#if defined(CYUI_HAS_CODEEDITOR)
    {
        CodeEditor editor;
        const std::string text = "\tconst char* label = \"音楽\";";
        editor.setText(text);
        check(editor.text() == text, "editor load preserves tabs, UTF-8 and final newline");
        editor.setText("a\r\nb\r\n");
        check(editor.text() == "a\r\nb\r\n", "editor CRLF preservation");
        editor.native().SetText("c\nd\n");
        check(editor.text() == "c\r\nd\r\n", "editor CRLF after an edit");
        editor.setText("a\nb");
        editor.native().SetText("c\nd");
        check(editor.text() == "c\nd", "editor LF after an edit");
        editor.setText(text);
        editor.setLanguage(CodeLanguage::cpp);
        frame([&] { editor.draw("Editor", {500, 300}); });
        check(editor.text() == text, "editor display preserves text");
    }
#endif
#if defined(CYUI_HAS_INSPECTOR)
    InspectorState inspectState;
    Property property;
    property.id = 1;
    property.label = "Exact";
    property.value = std::int64_t(9007199254740993LL);
    frame([&] { inspector("Inspector", inspectState, {&property, 1}); });
    check(std::get<std::int64_t>(inspectState.drafts[1].value) == 9007199254740993LL,
          "inspector retains 64-bit precision");
#endif
#if defined(CYUI_HAS_ASSETS)
    AssetState assets;
    std::vector<AssetItem> items{{1, 0, "Folder", "Folder", 0, true}, {2, 0, "Item", "Image"}};
    frame([&] { assetBrowser("Assets", assets, items, {500, 300}); });
#endif
#if defined(CYUI_HAS_DOCUMENTVIEWS)
    Document document;
    document.setMarkdown("# Heading\n```cpp\nint x=1;\n```\n> Note\n");
    check(document.blocks().size() == 3 && document.blocks()[1].kind == DocumentBlockKind::code,
          "document block parser");
    DocumentState documentState;
    frame([&] { documentView("Document", documentState, document, {500, 400}); });
#endif
    ImGui::DestroyContext();
    if (!failures)
        std::puts("CyUI models and interactions passed");
    return failures ? 1 : 0;
}
