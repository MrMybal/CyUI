#include <array>
#include <cstdio>
#include <cstdlib>
#include <cyui/modules/DocumentViews.h>
#include <fstream>
#include <imgui_internal.h>
#include <iterator>
#include <map>
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
    if (std::getenv("CYUI_TEST_MAC_SHORTCUTS"))
        io.ConfigMacOSXBehaviors = true;
    const auto shortcut = io.ConfigMacOSXBehaviors ? ImGuiMod_Super : ImGuiMod_Ctrl;
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
    MessageComposerState draft;
    MessageComposerOptions composeOptions;
    composeOptions.attachments = false;
    MessageComposerResult composed;
    auto drawComposer = [&] { composed = messageComposer("chat-input", draft, {600, 80}, composeOptions); };
    draft.focus = true;
    frame(drawComposer);
    frame(drawComposer);
    io.AddInputCharactersUTF8("été");
    frame(drawComposer);
    check(draft.text == "été", "chat composer accepts native UTF-8 input");
    io.AddKeyEvent(ImGuiMod_Shift, true);
    io.AddKeyEvent(ImGuiKey_Enter, true);
    frame(drawComposer);
    check(!composed.submitted && draft.text.find('\n') != std::string::npos,
          "Shift+Enter inserts a line without sending");
    io.AddKeyEvent(ImGuiKey_Enter, false);
    io.AddKeyEvent(ImGuiMod_Shift, false);
    frame(drawComposer);
    const auto beforeSend = draft.text;
    io.AddKeyEvent(ImGuiKey_Enter, true);
    frame(drawComposer);
    check(composed.submitted && draft.text == beforeSend, "Enter submits without inserting a newline");
    io.AddKeyEvent(ImGuiKey_Enter, false);
    frame(drawComposer);
    io.AddKeyEvent(ImGuiKey_Escape, true);
    frame(drawComposer);
    check(composed.cancelled && draft.text == beforeSend,
          "Escape requests cancellation without reverting the draft");
    io.AddKeyEvent(ImGuiKey_Escape, false);
    frame(drawComposer);
    draft.text = "été";
    draft.focus = true;
    frame(drawComposer);
    frame(drawComposer);
    if (auto *input = ImGui::GetInputTextState(draft.input))
        input->SelectAll();
    frame(drawComposer);
    io.AddKeyEvent(shortcut, true);
    io.AddKeyEvent(ImGuiKey_B, true);
    frame(drawComposer);
    io.AddKeyEvent(ImGuiKey_B, false);
    io.AddKeyEvent(shortcut, false);
    frame(drawComposer);
    check(draft.text == "**été**", "chat formatting preserves UTF-8 selection");
    composeOptions.paste = [](std::string &text) {
        text = "replacement";
        return true;
    };
    if (auto *input = ImGui::GetInputTextState(draft.input))
        input->SelectAll();
    frame(drawComposer);
    io.AddKeyEvent(shortcut, true);
    io.AddKeyEvent(ImGuiKey_V, true);
    frame(drawComposer);
    io.AddKeyEvent(ImGuiKey_V, false);
    io.AddKeyEvent(shortcut, false);
    frame(drawComposer);
    check(draft.text == "replacement", "rich paste replaces selection exactly once");
    draft.text.clear();
    frame(drawComposer);
    frame(drawComposer);
    check(draft.text.empty(), "sending can clear an active native composer without restoring stale text");
    draft.focus = true;
    frame(drawComposer);
    frame(drawComposer);
    const bool trickle = io.ConfigInputTrickleEventQueue;
    io.ConfigInputTrickleEventQueue = false;
    io.AddInputCharactersUTF8("fast");
    io.AddKeyEvent(ImGuiKey_Enter, true);
    frame(drawComposer);
    check(composed.submitted && draft.text == "fast",
          "text and Enter in the same frame send the complete message");
    io.AddKeyEvent(ImGuiKey_Enter, false);
    frame(drawComposer);
    io.ConfigInputTrickleEventQueue = trickle;
    TranscriptState nativeChat;
    std::vector<TranscriptMessage> chatMessages{{41, "User", "__under__ `__code__`", "12:00"}};
    frame([&] { transcript("native-chat", nativeChat, chatMessages, {700, 400}); });
    bool chatUnderline = false, untouchedCode = false;
    for (const auto &block : nativeChat.documents[41].blocks())
        for (const auto &run : block.inlines) {
            chatUnderline |= run.text == "under" && (run.style & documentUnderline);
            untouchedCode |= run.text == "__code__" && (run.style & documentMono);
        }
    check(chatUnderline && untouchedCode, "Discord underline is chat-only and leaves inline code unchanged");
    TranscriptResult chatAction;
    ImVec2 editPoint{};
    auto drawChat = [&] {
        chatAction = transcript("native-chat", nativeChat, chatMessages, {700, 400});
        for (auto *window : ImGui::GetCurrentWindow()->DC.ChildWindows)
            if (std::string(window->Name).find("##transcript") != std::string::npos) {
                const auto button = ImGui::GetFrameHeight() + dp(2);
                editPoint = {window->WorkRect.Max.x - ImGui::GetStyle().ScrollbarSize - 2 * button - dp(2) +
                                 button * .5f,
                             window->DC.CursorStartPos.y + ImGui::GetFrameHeight() * .5f};
            }
    };
    frame(drawChat);
    frame(drawChat, editPoint);
    check(nativeChat.hovered == 41, "message actions appear only for the hovered row");
    frame(drawChat, editPoint, 0, true);
    frame(drawChat, editPoint, 0, false);
    check(chatAction.edit == 41 && chatMessages[0].content == "__under__ `__code__`",
          "small native edit icon requests an edit without mutating the host message");
    nativeChat.views[41].enlargedImage = "assets/picture.png";
    frame([&] { transcript("native-chat", nativeChat, chatMessages, {700, 400}, options); });
    frame([&] { transcript("native-chat", nativeChat, chatMessages, {700, 400}, options); }, {-100, -100}, -1,
          false, true);
    check(nativeChat.views[41].enlargedImage.empty(),
          "chat image lightbox is interactive and closes with Escape");
    {
        TranscriptState large;
        large.follow = false;
        TranscriptSource history;
        history.id = 700;
        history.count = 1'000'000;
        TranscriptOptions virtualOptions;
        virtualOptions.cacheMessages = 16;
        TranscriptMessage row;
        std::size_t reads = 0, requests = 0, requestedRows = 0;
        bool loaded = true;
        history.message = [&](std::size_t index) -> const TranscriptMessage * {
            ++reads;
            if (!loaded)
                return nullptr;
            row = {ItemId(index + 1), "User", "Message **" + std::to_string(index + 1) + "**", "12:00"};
            return &row;
        };
        history.indexOf = [](ItemId id) { return std::size_t(id - 1); };
        history.request = [&](std::size_t first, std::size_t count) {
            ++requests;
            requestedRows = std::max(requestedRows, count);
            check(first <= history.count && count <= history.count - first,
                  "paged chat requests stay inside the history");
        };
        auto drawLarge = [&] { transcript("million-chat", large, history, {700, 400}, virtualOptions); };
        for (int n = 0; n < 3; ++n)
            frame(drawLarge);
        check(reads < 80 && large.documents.size() <= 16 && large.heightIndex.size() < 300 && requests > 0 &&
                  requestedRows < 40,
              "one million chat rows fetch/parse only the viewport with sparse height metadata");
        ImVec2 scrollbarPoint{};
        for (auto *child : ImGui::FindWindowByName("Documents")->DC.ChildWindows)
            if (child->Active && std::string(child->Name).find("##transcript") != std::string::npos) {
                scrollbarPoint = {child->WorkRect.Max.x - ImGui::GetStyle().ScrollbarSize * .5f,
                                  (child->WorkRect.Min.y + child->WorkRect.Max.y) * .5f};
            }
        io.AddKeyEvent(ImGuiMod_Shift, true);
        frame(drawLarge, scrollbarPoint);
        frame(drawLarge, scrollbarPoint, 0, true);
        frame(drawLarge, scrollbarPoint, 0, false);
        io.AddKeyEvent(ImGuiMod_Shift, false);
        frame(drawLarge, scrollbarPoint);
        check(large.firstVisible > 200'000 && large.firstVisible < 800'000,
              "native scrollbar jumps into the middle of a million-row virtual history");
        reads = 0;
        large.scrollToIndex = history.count - 2;
        for (int n = 0; n < 3; ++n)
            frame(drawLarge);
        check(large.firstVisible > 999'900 && large.lastVisible == history.count && reads < 80,
              "virtual chat jumps directly to the end of a million-row history");
        frame(drawLarge, {100, 100});
        const auto beforeWheel = large.scroll;
        io.AddMouseWheelEvent(0, 1);
        frame(drawLarge, {100, 100});
        check(beforeWheel - large.scroll > dp(20) && beforeWheel - large.scroll < dp(200),
              "chat wheel movement keeps pixel precision at large logical scroll offsets");
        for (std::size_t n = 0; n < 12; ++n) {
            large.scrollToIndex = n * 70'000;
            frame(drawLarge);
        }
        check(
            large.documents.size() <= 16 && large.views.size() <= 16 && large.heights.size() <= 16,
            "scrolling a large history evicts parsed documents and interaction state from the bounded cache");
        large.jumpToBottom = true;
        for (int n = 0; n < 3; ++n)
            frame(drawLarge);
        large.follow = true;
        history.count += 10;
        for (int n = 0; n < 3; ++n)
            frame(drawLarge);
        check(large.lastVisible == history.count && large.scroll == large.scrollMax,
              "virtual chat follows appended messages while the reader is at the bottom");
        history.count = 1'048'575;
        for (int n = 0; n < 3; ++n)
            frame(drawLarge);
        history.count += 2;
        for (int n = 0; n < 3; ++n)
            frame(drawLarge);
        check(large.lastVisible == history.count && large.scroll == large.scrollMax &&
                  large.heightCapacity == 2'097'152,
              "growing chat height indexing preserves measured heights across capacity boundaries");
        loaded = false;
        history.id = 701;
        large.follow = false;
        large.scrollToIndex = 500'000;
        frame(drawLarge);
        check(large.documents.empty() && large.fetched < 30 && large.firstVisible >= 500'000,
              "unloaded chat pages render viewport placeholders without loading the history");
        loaded = true;
        frame(drawLarge);
        check(!large.documents.empty() && large.firstVisible >= 500'000,
              "host-supplied chat pages replace placeholders at the same logical scroll position");
    }
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
    for (bool soft : {false, true}) {
        editor.setMarkdown("## Heading\n\ntestds\n\n- [x] Keep task\n");
        editor.select(10, 10);
        editor.focus();
        for (int n = 0; n < 3; ++n)
            frame(drawLive);
        const auto headingId = GImGui->ActiveId;
        ImGuiWindow *headingWindow = nullptr;
        for (auto *child : GImGui->Windows)
            if (child->ChildId == headingId)
                headingWindow = child;
        io.AddKeyEvent(ImGuiMod_Shift, soft);
        io.AddKeyEvent(ImGuiKey_Enter, true);
        frame(drawLive);
        io.AddKeyEvent(ImGuiKey_Enter, false);
        io.AddKeyEvent(ImGuiMod_Shift, false);
        bool titleVisible = true;
        for (int n = 0; n < 8; ++n) {
            frame(drawLive);
            if (headingWindow) {
                int visibleVertices = 0;
                for (const auto &vertex : headingWindow->DrawList->VtxBuffer)
                    if (vertex.col == palette().text && headingWindow->InnerClipRect.Contains(vertex.pos))
                        ++visibleVertices;
                titleVisible &= visibleVertices > 8;
                titleVisible &= headingWindow->Scroll.y == 0;
            }
        }
        check(titleVisible,
              "heading glyphs remain visible during the idle frames after a line/paragraph break");
        if (soft) {
            for (const char *word : {"gfdslkhj", "jkjhg", "hjgf", "kjgh"}) {
                io.AddInputCharactersUTF8(word);
                frame(drawLive);
                io.AddKeyEvent(ImGuiMod_Shift, true);
                io.AddKeyEvent(ImGuiKey_Enter, true);
                frame(drawLive);
                io.AddKeyEvent(ImGuiKey_Enter, false);
                io.AddKeyEvent(ImGuiMod_Shift, false);
                for (int n = 0; n < 4; ++n)
                    frame(drawLive);
            }
            const auto beforeBlur = editor.markdown();
            const auto beforeHeight = headingWindow ? headingWindow->Size.y : 0;
            editor.select(beforeBlur.find("testds"), beforeBlur.find("testds"));
            editor.focus();
            for (int n = 0; n < 4; ++n)
                frame(drawLive);
            check(editor.markdown() == "## Heading<br>gfdslkhj<br>jkjhg<br>hjgf<br>kjgh<br>\n\ntestds\n\n- "
                                       "[x] Keep task\n" &&
                      editor.markdown() == beforeBlur && headingWindow &&
                      headingWindow->Size.y == beforeHeight && headingWindow->Scroll.y == 0,
                  "the video's repeated soft breaks keep text, height and heading layout stable after "
                  "leaving the block");
        }
    }
    editor.setMarkdown("## Heading\n\ntestds\n\n- [x] Keep task\n");
    editor.select(10, 10);
    editor.focus();
    for (int n = 0; n < 3; ++n)
        frame(drawLive);
    std::string paragraphs;
    for (const char *word : {"gfdslkhj", "jkjhg", "hjgf", "kjgh"}) {
        io.AddKeyEvent(ImGuiKey_Enter, true);
        frame(drawLive);
        io.AddKeyEvent(ImGuiKey_Enter, false);
        for (int n = 0; n < 5; ++n)
            frame(drawLive);
        io.AddInputCharactersUTF8(word);
        for (int n = 0; n < 3; ++n)
            frame(drawLive);
        paragraphs += "\n\n" + std::string(word);
    }
    const auto richLayout = [] {
        std::map<ImGuiID, std::array<float, 5>> result;
        for (const auto *window : GImGui->Windows)
            if (window->Active && std::string(window->Name).find("##rich") != std::string::npos)
                result[window->ID] = {window->Pos.x, window->Pos.y, window->Size.x, window->Size.y,
                                      window->Scroll.y};
        return result;
    };
    const auto paragraphsLayout = richLayout();
    const auto paragraphsSource = editor.markdown();
    editor.select(0, 0);
    editor.focus();
    for (int n = 0; n < 4; ++n)
        frame(drawLive);
    check(paragraphsSource == "## Heading" + paragraphs + "\n\ntestds\n\n- [x] Keep task\n" &&
              editor.markdown() == paragraphsSource && paragraphsLayout == richLayout(),
          "repeated Enter creates ordered paragraphs with identical geometry after focus changes");
    editor.setMarkdown("Start\n\ntestds\n");
    editor.select(5, 5);
    editor.focus();
    for (int n = 0; n < 3; ++n)
        frame(drawLive);
    io.ConfigInputTrickleEventQueue = false;
    io.AddInputCharactersUTF8("fast");
    io.AddKeyEvent(ImGuiKey_Enter, true);
    frame(drawLive);
    io.AddKeyEvent(ImGuiKey_Enter, false);
    for (int n = 0; n < 4; ++n)
        frame(drawLive);
    io.ConfigInputTrickleEventQueue = trickle;
    check(editor.markdown() == "Startfast\n\n\n\ntestds\n",
          "text and Enter delivered in one frame split after the newly typed text");
    editor.setMarkdown("**Bonjour** à tous.\n\nSecond paragraph.\n");
    frame(drawLive);
    frame(drawLive, {65, 45}, 0, true);
    frame(drawLive, {65, 45}, 0, false);
    io.AddKeyEvent(shortcut, true);
    io.AddKeyEvent(ImGuiKey_A, true);
    frame(drawLive);
    io.AddKeyEvent(ImGuiKey_A, false);
    io.AddKeyEvent(shortcut, false);
    frame(drawLive);
    io.AddInputCharactersUTF8("Salut");
    frame(drawLive);
    check(editor.markdown() == "Salut", "Live Ctrl+A selects the whole document across paragraphs");
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
    check(editor.markdown() == listEdit && listEdit.find("- Second") != std::string::npos &&
              listEdit.find("\n") == listEdit.find("\r\n") + 1,
          "Live list continuation starts a new item and preserves CRLF");
    editor.setMarkdown("");
    editor.focus();
    frame(drawLive);
    frame(drawLive);
    frame(drawLive);
    io.AddInputCharactersUTF8("# Titre");
    frame(drawLive);
    frame(drawLive);
    frame(drawLive);
    Document typedHeading;
    typedHeading.setMarkdown(editor.markdown());
    check(typedHeading.blocks().size() == 1 && typedHeading.blocks()[0].kind == DocumentBlockKind::heading,
          "typing Markdown creates a heading in Live without exposing or escaping its markers");
    io.AddKeyEvent(ImGuiKey_Enter, true);
    frame(drawLive);
    io.AddKeyEvent(ImGuiKey_Enter, false);
    frame(drawLive);
    io.AddInputCharactersUTF8("**texte**");
    frame(drawLive);
    frame(drawLive);
    frame(drawLive);
    Document typedBold;
    typedBold.setMarkdown(editor.markdown());
    bool liveBold = false;
    for (const auto &block : typedBold.blocks())
        for (const auto &run : block.inlines)
            liveBold |= run.text == "texte" && (run.style & documentBold);
    check(liveBold && typedBold.blocks().size() == 2,
          "Live Enter exits the heading and typed delimiters create rich text");
    editor.setMarkdown("First.\n\nSecond.\n");
    editor.select(8, 8);
    editor.focus();
    frame(drawLive);
    frame(drawLive);
    frame(drawLive);
    io.AddKeyEvent(ImGuiKey_Backspace, true);
    frame(drawLive);
    io.AddKeyEvent(ImGuiKey_Backspace, false);
    frame(drawLive);
    check(editor.markdown() == "First.Second.\n", "Backspace joins adjacent Live paragraphs at the caret");
    const std::string insertionSource = "Alpha **bold** omega.\n\nKeep this paragraph.\n";
    editor.setMarkdown(insertionSource);
    const auto insertionAt = insertionSource.find("omega") + 2;
    editor.select(insertionAt, insertionAt);
    editor.focus();
    for (int n = 0; n < 3; ++n)
        frame(drawLive);
    std::string insertedText;
    const auto typingInput = GImGui->ActiveId;
    bool insertionStable = true, insertionCollapsed = true, insertionFocus = typingInput != 0;
    for (const char *character : {"x", "y", "é", "z", " ", "a", " ", "b"}) {
        insertedText += character;
        io.AddInputCharactersUTF8(character);
        frame(drawLive);
        insertionStable &= editor.markdown() == insertionSource.substr(0, insertionAt) + insertedText +
                                                    insertionSource.substr(insertionAt);
        insertionCollapsed &= editor.selection().first == editor.selection().second;
        insertionFocus &= GImGui->ActiveId == typingInput;
        frame(drawLive);
    }
    check(insertionStable,
          "character-by-character Live typing inserts at the caret without overwriting surrounding text");
    check(insertionCollapsed, "Live typing leaves a collapsed caret, not a formatting selection");
    check(insertionFocus, "ordinary Live typing keeps the same active native input");
    editor.setMarkdown("Start\n\nKeep.\n");
    editor.select(5, 5);
    editor.focus();
    for (int n = 0; n < 3; ++n)
        frame(drawLive);
    std::string typedSuffix;
    bool typedSyntaxStable = true;
    for (const char *characters : {" ", "**", "bold", "**", " ", "é", " ", "fin."}) {
        typedSuffix += characters;
        io.AddInputCharactersUTF8(characters);
        frame(drawLive);
        frame(drawLive);
        frame(drawLive);
        typedSyntaxStable &= editor.markdown() == "Start" + typedSuffix + "\n\nKeep.\n";
    }
    check(typedSyntaxStable && GImGui->ActiveId != 0,
          "trailing spaces and closing Markdown delimiters keep Live input focused for subsequent words");
    editor.setMarkdown("First line.\n\nKeep this paragraph.\n");
    editor.select(11, 11);
    editor.focus();
    for (int n = 0; n < 3; ++n)
        frame(drawLive);
    io.AddKeyEvent(ImGuiMod_Shift, true);
    io.AddKeyEvent(ImGuiKey_Enter, true);
    frame(drawLive);
    const auto softBreakSelection = editor.selection();
    bool softBreakToolbar = false;
    for (const auto *window : GImGui->Windows)
        softBreakToolbar |= window->Active && std::string(window->Name).find("##documentFormat") == 0;
    io.AddKeyEvent(ImGuiKey_Enter, false);
    io.AddKeyEvent(ImGuiMod_Shift, false);
    frame(drawLive);
    for (const char *character : {"N", "e", "w"}) {
        io.AddInputCharactersUTF8(character);
        frame(drawLive);
        frame(drawLive);
    }
    check(softBreakSelection.first == softBreakSelection.second && !softBreakToolbar,
          "Shift+Enter keeps a caret and does not open the formatting toolbar");
    check(editor.markdown() == "First line.<br>New\n\nKeep this paragraph.\n",
          "Shift+Enter inserts one line break and subsequent typing preserves following paragraphs");
    editor.setMarkdown("Zero **un mot** end.\n\nKeep.\n");
    editor.select(7, 7);
    editor.focus();
    for (int n = 0; n < 3; ++n)
        frame(drawLive);
    if (auto *input = ImGui::GetInputTextState(GImGui->ActiveId))
        input->SetSelection(5, 7);
    frame(drawLive);
    io.AddInputCharactersUTF8("é");
    frame(drawLive);
    frame(drawLive);
    check(editor.markdown() == "Zero **é mot** end.\n\nKeep.\n" &&
              editor.selection().first == editor.selection().second,
          "replacing a native selection preserves rich styles and following blocks");
    io.AddKeyEvent(ImGuiKey_Delete, true);
    frame(drawLive);
    io.AddKeyEvent(ImGuiKey_Delete, false);
    frame(drawLive);
    io.AddKeyEvent(ImGuiKey_Backspace, true);
    frame(drawLive);
    io.AddKeyEvent(ImGuiKey_Backspace, false);
    frame(drawLive);
    check(editor.markdown() == "Zero **mot** end.\n\nKeep.\n",
          "Delete and Backspace edit at the collapsed UTF-8 caret without replacing the paragraph");
    const auto beforeUndo = editor.markdown();
    editor.command(DocumentCommand::undo);
    frame(drawLive);
    check(editor.markdown() != beforeUndo && editor.markdown().find("Keep.") != std::string::npos,
          "undo restores a native Live edit without losing subsequent paragraphs");
    editor.command(DocumentCommand::redo);
    frame(drawLive);
    frame(drawLive);
    check(editor.markdown() == beforeUndo, "redo keeps the native Live buffer synchronized");
    editor.setMarkdown("- First\r\n\r\nKeep.\r\n");
    editor.select(7, 7);
    editor.focus();
    for (int n = 0; n < 3; ++n)
        frame(drawLive);
    io.AddKeyEvent(ImGuiMod_Shift, true);
    io.AddKeyEvent(ImGuiKey_Enter, true);
    frame(drawLive);
    io.AddKeyEvent(ImGuiKey_Enter, false);
    io.AddKeyEvent(ImGuiMod_Shift, false);
    frame(drawLive);
    io.AddInputCharactersUTF8("continued");
    frame(drawLive);
    frame(drawLive);
    frame(drawLive);
    Document continuedList;
    continuedList.setMarkdown(editor.markdown());
    check(continuedList.blocks().size() == 2 && continuedList.blocks()[0].kind == DocumentBlockKind::list &&
              RichText(continuedList.blocks()[0].inlines).text() == "First\ncontinued" &&
              editor.markdown().find("\r\n") != std::string::npos,
          "Shift+Enter continues the same list item and preserves CRLF");
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
    io.AddKeyEvent(shortcut, true);
    io.AddKeyEvent(ImGuiKey_V, true);
    frame(drawLive);
    io.AddKeyEvent(ImGuiKey_V, false);
    io.AddKeyEvent(shortcut, false);
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
