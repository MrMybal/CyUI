#include <cstdio>
#include <cyui/modules/DocumentViews.h>
#include <fstream>
#include <imgui_internal.h>
#include <iterator>
#if defined(CYUI_HAS_DOCUMENTIO)
#include <cyui/modules/DocumentIO.h>
#endif
#if defined(CYUI_HAS_DOCUMENTEDITOR)
#include <cyui/modules/DocumentEditor.h>
#endif
#if defined(CYUI_HAS_DOCUMENTWEB)
#include <cyui/modules/DocumentWeb.h>
#endif
namespace {
int failures = 0;
void check(bool value, const char *message) {
    if (!value) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
}
template <class F>
void frame(F draw, ImVec2 mouse = {-100, -100}, int button = -1, bool down = false, bool escape = false) {
    auto &io = ImGui::GetIO();
    io.AddMousePosEvent(mouse.x, mouse.y);
    if (button >= 0)
        io.AddMouseButtonEvent(button, down);
    if (escape)
        io.AddKeyEvent(ImGuiKey_Escape, true);
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({1000, 800});
    ImGui::Begin("Documents", nullptr, ImGuiWindowFlags_NoSavedSettings);
    draw();
    ImGui::End();
    ImGui::Render();
    if (escape)
        io.AddKeyEvent(ImGuiKey_Escape, false);
}
} // namespace
int main(int argc, char **argv) {
    using namespace cyui;
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1000, 800};
    io.DeltaTime = 1.f / 60;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    apply();
    unsigned char *pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID(ImTextureID(1));
    Document doc;
    const std::string source =
        "# Title\n\n**Bold *nested*** <u>under</u> ~~gone~~ ||secret|| <mark>marked</mark> <kbd>Ctrl</kbd> "
        "&amp; &#233;\n\n3. Third\n   - Nested\n- [ ] Task\n\n| A | B |\n|:---|---:|\n| x\\|y | **z** "
        "|\n\n![Landscape|320x180](assets/picture.png)\n\n[reference][id]\n\n[id]: "
        "https://example.com\n\n```rust\nlet x = 1;\n```\n";
    check(doc.setMarkdown(source), "new document parsed");
    check(!doc.setMarkdown(source), "unchanged source preserves revision");
    bool nested = false, under = false, strike = false, spoiler = false, mark = false, kbd = false,
         image = false, reference = false, task = false, table = false, list = false, code = false;
    std::size_t taskOffset = 0;
    for (const auto &b : doc.blocks()) {
        if (b.task) {
            task = true;
            taskOffset = b.taskOffset;
        }
        if (b.number == 3)
            list = true;
        if (b.kind == DocumentBlockKind::table)
            table = b.rows.size() == 2 && b.rows[1][0] == "x|y" && b.alignments[1] == 3;
        if (b.kind == DocumentBlockKind::code)
            code = b.language == "rust";
        for (const auto &r : b.inlines) {
            nested |= (r.style & (documentBold | documentItalic)) == (documentBold | documentItalic);
            under |= (r.style & documentUnderline) != 0;
            strike |= (r.style & documentStrike) != 0;
            spoiler |= (r.style & documentSpoiler) != 0;
            mark |= (r.style & documentHighlight) != 0;
            kbd |= (r.style & documentKeyboard) != 0;
            image |= r.kind == DocumentInlineKind::image && r.width == 320 && r.height == 180 &&
                     r.target == "assets/picture.png";
            reference |= r.text == "reference" && r.target == "https://example.com";
        }
    }
    check(nested && under && strike && spoiler && mark && kbd, "composable native inline styles");
    check(task && source[taskOffset] == ' ', "task offset maps exact source checkbox");
    check(table && list && code && image && reference,
          "GFM nesting, escaped tables, languages, sized images, reference links");
    const auto html = documentHtml(doc, "<Title>");
    check(html.find("&lt;Title&gt;") != std::string::npos && html.find("<script>") == std::string::npos,
          "HTML export escapes title");
    Document unsafe;
    unsafe.setMarkdown("<script>alert(1)</script>\n\n[bad](javascript:alert)\n");
    check(documentHtml(unsafe).find("href=\"javascript:") == std::string::npos,
          "export never activates unsafe links");
    DocumentState view;
    DocumentResult result;
    DocumentServices services;
    int images = 0, embeds = 0;
    services.image = [&](const std::string &) {
        ++images;
        return ImageData{ImTextureID(2), 640, 360};
    };
    services.embed = [&](const DocumentMedia &, ImVec2) {
        ++embeds;
        return true;
    };
    DocumentOptions options;
    options.services = &services;
    frame([&] { result = documentView("doc", view, doc, {650, 650}, options); });
    check(images > 0 && embeds == 0, "images draw natively, optional embeds never activate during rendering");
    check(!view.hits.empty() && view.anchors.count("title"),
          "native text hit map and heading anchors available");
    Document secrets;
    secrets.setMarkdown("||secret|| visible");
    DocumentState secretState;
    frame([&] { documentView("secret", secretState, secrets, {500, 200}); });
    bool leaked = false;
    for (const auto &h : secretState.hits)
        leaked |= h.text.find("secret") != std::string::npos;
    check(!leaked, "hidden spoilers are excluded from selectable plaintext");
    view.enlargedImage = "assets/picture.png";
    frame([&] { documentView("doc", view, doc, {650, 650}, options); });
    frame([&] { documentView("doc", view, doc, {650, 650}, options); }, {-100, -100}, -1, false, true);
    check(view.enlargedImage.empty(), "native image lightbox closes with Escape");
#if defined(CYUI_HAS_DOCUMENTEDITOR)
    RichText rich{{{DocumentInlineKind::strong, "héllo", "", false, documentBold}}};
    check(!rich.replace(2, 3, "x"), "rich edits reject mid-codepoint UTF-8 positions");
    check(rich.replace(3, 5, "y") && rich.text() == "héyo" && rich.markdown() == "**héyo**",
          "rich replacement preserves styles and UTF-8");
    DocumentEditor editor;
    editor.setMarkdown("Hello world\n");
    check(editor.select(6, 11) && editor.command(DocumentCommand::bold),
          "format command accepts source selection");
    check(editor.markdown() == "Hello **world**\n" && editor.canUndo(),
          "formatting is a native undoable transaction");
    editor.command(DocumentCommand::undo);
    check(editor.markdown() == "Hello world\n", "undo restores exact document");
    editor.command(DocumentCommand::redo);
    check(editor.markdown() == "Hello **world**\n", "redo restores format");
    editor.setMarkdown("A &amp; B and \\*literal\\*.\n");
    check(editor.select(2, 7) && editor.command(DocumentCommand::bold), "format an entity in Live");
    Document formatted;
    formatted.setMarkdown(editor.markdown());
    bool formattedAmp = false;
    for (const auto &block : formatted.blocks())
        for (const auto &run : block.inlines)
            formattedAmp |= run.text == "&" && (run.style & documentBold);
    check(formattedAmp && formatted.plainText().find("*literal*") != std::string::npos,
          "Live formatting preserves entity text and escaped literal markers");
    editor.setMarkdown(source);
    for (auto mode : {DocumentMode::live, DocumentMode::source, DocumentMode::split, DocumentMode::reading}) {
        editor.setMode(mode);
        frame([&] { editor.draw("editor", {950, 700}, {options}); });
    }
    check(editor.markdown() == source, "switching between all modes preserves source");
    editor.setMarkdown("**Bonjour** à tous.\n\nSecond paragraph.\n");
    editor.setMode(DocumentMode::live);
    DocumentEditorOptions liveOptions;
    liveOptions.toolbar = liveOptions.modeSelector = false;
    auto drawLive = [&] { editor.draw("typing", {800, 600}, liveOptions); };
    frame(drawLive);
    frame(drawLive, {65, 45}, 0, true);
    frame(drawLive, {65, 45}, 0, false);
    io.AddKeyEvent(ImGuiMod_Ctrl, true);
    io.AddKeyEvent(ImGuiKey_A, true);
    frame(drawLive);
    io.AddKeyEvent(ImGuiKey_A, false);
    io.AddKeyEvent(ImGuiMod_Ctrl, false);
    frame(drawLive);
    io.AddInputCharactersUTF8("Salut");
    frame(drawLive);
    check(editor.markdown().find("**Salut**") != std::string::npos &&
              editor.markdown().find("Second paragraph.") != std::string::npos,
          "live native text input preserves formatting and unaffected paragraphs");
    editor.command(DocumentCommand::undo);
    frame(drawLive);
    check(editor.markdown() == "**Bonjour** à tous.\n\nSecond paragraph.\n",
          "live edit and undo share the source transaction history");
    editor.setMarkdown("First.\n\nLast.\n");
    frame(drawLive);
    frame(drawLive, {65, 45}, 0, true);
    frame(drawLive, {65, 45}, 0, false);
    io.AddKeyEvent(ImGuiKey_End, true);
    frame(drawLive);
    io.AddKeyEvent(ImGuiKey_End, false);
    frame(drawLive);
    io.AddKeyEvent(ImGuiKey_Enter, true);
    frame(drawLive);
    io.AddKeyEvent(ImGuiKey_Enter, false);
    frame(drawLive);
    const auto afterEnter = editor.markdown();
    for (int n = 0; n < 4; ++n)
        frame(drawLive);
    check(editor.markdown() == afterEnter, "a Live newline does not change the document during idle frames");
    io.AddInputCharactersUTF8("New");
    frame(drawLive);
    io.AddKeyEvent(ImGuiKey_Enter, true);
    frame(drawLive);
    io.AddKeyEvent(ImGuiKey_Enter, false);
    frame(drawLive);
    io.AddKeyEvent(ImGuiKey_Enter, true);
    frame(drawLive);
    io.AddKeyEvent(ImGuiKey_Enter, false);
    frame(drawLive);
    io.AddInputCharactersUTF8("Next");
    frame(drawLive);
    const auto afterParagraph = editor.markdown();
    for (int n = 0; n < 4; ++n)
        frame(drawLive);
    check(editor.markdown() == afterParagraph &&
              afterParagraph.find("Last.") == afterParagraph.rfind("Last.") &&
              afterParagraph.find("Next") != std::string::npos,
          "Live paragraph creation is stable and keeps following blocks");
    frame(drawLive, {950, 750}, 0, true);
    frame(drawLive, {950, 750}, 0, false);
    check(editor.markdown() == afterParagraph, "leaving Live input does not reapply its old grouped buffer");
    editor.setMarkdown("- **First**\r\n\r\nLast.\r\n");
    frame(drawLive);
    frame(drawLive, {90, 45}, 0, true);
    frame(drawLive, {90, 45}, 0, false);
    io.AddKeyEvent(ImGuiKey_End, true);
    frame(drawLive);
    io.AddKeyEvent(ImGuiKey_End, false);
    frame(drawLive);
    io.AddKeyEvent(ImGuiKey_Enter, true);
    frame(drawLive);
    io.AddKeyEvent(ImGuiKey_Enter, false);
    frame(drawLive);
    io.AddInputCharactersUTF8("Second");
    frame(drawLive);
    const auto listEdit = editor.markdown();
    for (int n = 0; n < 3; ++n)
        frame(drawLive);
    check(editor.markdown() == listEdit && listEdit.find("- **Second**") != std::string::npos &&
              listEdit.find("\n") == listEdit.find("\r\n") + 1,
          "Live list continuation preserves inherited styles and CRLF");
    editor.setMarkdown("Loaded while focused.\n");
    for (int n = 0; n < 3; ++n)
        frame(drawLive);
    check(editor.markdown() == "Loaded while focused.\n",
          "loading a document discards the previous active input");
    editor.setMarkdown("");
    frame(drawLive);
    frame(drawLive, {65, 45}, 0, true);
    frame(drawLive, {65, 45}, 0, false);
    io.AddInputCharactersUTF8("Empty document input");
    frame(drawLive);
    check(editor.markdown() == "Empty document input", "an empty document accepts native Live input");
    frame(drawLive, {950, 750}, 0, true);
    frame(drawLive, {950, 750}, 0, false);
    editor.setMarkdown("Floating selection");
    frame(drawLive);
    frame(drawLive, {65, 45}, 0, true);
    frame(drawLive, {65, 45}, 0, false);
    if (auto *input = ImGui::GetInputTextState(GImGui->ActiveId))
        input->SetSelection(0, 8);
    frame(drawLive);
    frame(drawLive); // Allow the newly appearing auto-sized toolbar to settle.
    ImGuiWindow *floating = nullptr;
    for (auto *window : GImGui->Windows)
        if (std::string(window->Name).find("##documentFormat") == 0 && window->Active)
            floating = window;
    check(floating != nullptr, "a native selection opens the floating formatting toolbar");
    if (floating) {
        const ImVec2 button{floating->Pos.x + 18, floating->Pos.y + 14};
        frame(drawLive, button, 0, true);
        frame(drawLive, button, 0, false);
        check(editor.markdown() == "**Floating** selection", "floating toolbar remains usable while focused");
    }
    liveOptions.floatingToolbar = false;
    liveOptions.pasteRich = [] { return std::string{}; };
    editor.setMarkdown("URL selection");
    frame(drawLive);
    frame(drawLive, {65, 45}, 0, true);
    frame(drawLive, {65, 45}, 0, false);
    if (auto *input = ImGui::GetInputTextState(GImGui->ActiveId))
        input->SetSelection(0, 3);
    frame(drawLive);
    auto &platform = ImGui::GetPlatformIO();
    const auto previousClipboard = platform.Platform_GetClipboardTextFn;
    platform.Platform_GetClipboardTextFn = [](ImGuiContext *) { return "https://example.com"; };
    io.AddKeyEvent(ImGuiMod_Ctrl, true);
    io.AddKeyEvent(ImGuiKey_V, true);
    frame(drawLive);
    io.AddKeyEvent(ImGuiKey_V, false);
    io.AddKeyEvent(ImGuiMod_Ctrl, false);
    frame(drawLive);
    platform.Platform_GetClipboardTextFn = previousClipboard;
    check(editor.markdown() == "[URL](<https://example.com>) selection",
          "URL paste falls back to link creation when the rich clipboard provider returns empty");
#endif
#if defined(CYUI_HAS_DOCUMENTWEB)
    DocumentWebAdapter web;
    check(!web.draw({1, DocumentMediaKind::web, "https://example.com", ""}, {300, 200}),
          "missing optional web backend returns native fallback");
#endif
#if defined(CYUI_HAS_DOCUMENTIO)
    DocumentPackage package;
    package.markdown = "# Résumé\r\n😀\r\n";
    package.assets.push_back({"assets/test.bin", {1, 2, 3, 4}});
    const auto archive = writeCyMD(package);
    const auto roundtrip = readCyMD(archive);
    check(roundtrip.markdown == package.markdown && roundtrip.assets[0].bytes == package.assets[0].bytes,
          "CyMD stored archive roundtrip preserves Unicode, CRLF and embedded resources");
    DocumentPackage illustrated;
    illustrated.markdown = "![photo](assets/photo.png)";
    illustrated.assets.push_back({"assets/photo.png", {1, 2, 3}});
    check(documentPackageHtml(illustrated, "<Title>").find("data:image/png;base64,") != std::string::npos,
          "package HTML export embeds referenced native image assets");
    for (auto encoding : {DocumentEncoding::utf8, DocumentEncoding::utf16LE, DocumentEncoding::utf16BE}) {
        package.encoding = encoding;
        package.bom = true;
        const auto bytes = encodeMarkdown(package);
        const auto decoded = decodeMarkdown(bytes);
        check(decoded.markdown == package.markdown && decoded.bom && decoded.encoding == encoding,
              "Markdown BOM/UTF-16/surrogate roundtrip");
    }
    bool rejected = false;
    try {
        auto bad = package;
        bad.assets.push_back({"../secret", {}});
        writeCyMD(bad);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    check(rejected, "archive rejects traversal paths");
    rejected = false;
    try {
        DocumentIOLimits tiny;
        tiny.expandedBytes = 2;
        readCyMD(archive, tiny);
    } catch (const std::length_error &) {
        rejected = true;
    }
    check(rejected, "archive expansion is bounded before allocation");
    const auto pasted = markdownFromHtml(
        "<p>Hello <strong>bold</strong></p><script>private()</script><img src='asset.png' alt='photo'>",
        [](const std::string &) { return "assets/photo.png"; });
    check(pasted.find("**bold**") != std::string::npos && pasted.find("private") == std::string::npos &&
              pasted.find("assets/photo.png") != std::string::npos,
          "HTML clipboard import handles formatting, explicit image import, script exclusion");
    Document clipboard;
    clipboard.setMarkdown(markdownFromHtml("<table><tr><th>A</th><th>B</th></tr>"
                                           "<tr><td><p>x|y</p></td><td>*literal*</td></tr></table>"));
    check(clipboard.blocks().size() == 1 && clipboard.blocks()[0].kind == DocumentBlockKind::table &&
              clipboard.blocks()[0].rows[1][0].find("x|y") != std::string::npos &&
              clipboard.blocks()[0].rows[1][1] == "*literal*",
          "clipboard HTML tables retain cells and literal Markdown characters");
    clipboard.setMarkdown(
        markdownFromHtml("<pre><code>if (a &lt; b) {\n  x = &quot;value&quot;;\n}</code></pre>"));
    check(clipboard.blocks()[0].kind == DocumentBlockKind::code &&
              clipboard.blocks()[0].text.find("if (a < b)") != std::string::npos &&
              clipboard.blocks()[0].text.find("\"value\"") != std::string::npos,
          "HTML clipboard code decodes entities while preserving its line breaks");
    if (argc > 1) {
        std::ifstream file(argv[1], std::ios::binary);
        std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(file),
                                         std::istreambuf_iterator<char>()};
        const auto compressed = readCyMD(bytes);
        check(compressed.assets.size() == 1 && compressed.markdown.find("Résumé") != std::string::npos,
              "imports DEFLATE archive produced by a separate ZIP implementation");
    }
#else
    (void)argc;
    (void)argv;
#endif
    ImGui::DestroyContext();
    return failures ? 1 : 0;
}
