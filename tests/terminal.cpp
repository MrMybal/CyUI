#include <cyui/modules/Terminal.h>
#include <cstdio>
#include <string_view>

namespace {

bool check(bool condition, const char *message) {
    if (!condition) std::fprintf(stderr, "%s\n", message);
    return condition;
}

bool keyboardTest(cyui::Terminal &terminal) {
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {640, 480};
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    cyui::apply({});
    cyui::loadFonts();
    unsigned char *pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID(ImTextureID(1));
    cyui::TerminalViewState view;
    const auto frame = [&](cyui::TerminalOptions options) {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({600, 440}, ImGuiCond_Always);
        ImGui::Begin("Test", nullptr, ImGuiWindowFlags_NoSavedSettings);
        cyui::terminalView("session", terminal, view, {550, 340}, options);
        ImGui::End();
        ImGui::Render();
    };

    frame({true, true});
    io.AddInputCharactersUTF8("\xc3\xa9");
    frame({});
    bool ok = check(terminal.takeInput() == "\xc3\xa9", "focused widget receives Unicode next frame");
    io.AddKeyEvent(ImGuiKey_Escape, true);
    frame({});
    ok &= check(terminal.takeInput() == "\x1b", "focused widget forwards Escape with ImGui navigation enabled");
    io.AddKeyEvent(ImGuiKey_Escape, false);
    io.AddKeyEvent(ImGuiMod_Ctrl, true);
    io.AddKeyEvent(ImGuiKey_C, true);
    frame({});
    ok &= check(terminal.takeInput() == "\x03", "Ctrl+C reaches the session as an interrupt");
    frame({false});
    io.AddInputCharactersUTF8("x");
    frame({false});
    ok &= check(terminal.takeInput().empty(), "read-only widget ignores typing");
    ImGui::DestroyContext();
    return ok;
}

} // namespace

int main() {
    cyui::Terminal term({30, 3}, 2);
    term.feed("\x1b[32mold\x1b[0m\r\none\r\ntwo\r\nthree\r\nfour\r\nfive");
    if (!check(term.text() == "one\ntwo\nthree\nfour\nfive", "bounded ordered scrollback")) return 1;
    term.feed("\x1b[?1049h\x1b[Hfull screen");
    if (!check(term.text().find("full screen") != std::string::npos && term.text().find("three") == std::string::npos,
               "alternate screen hides primary scrollback")) return 1;
    term.feed("\x1b[?1049l");
    if (!check(term.text() == "one\ntwo\nthree\nfour\nfive", "alternate screen restores the primary screen")) return 1;

    term.reset();
    term.feed("split \xc3");
    term.feed("\xa9");
    term.feed("\x1b[6n");
    if (!check(term.text() == "split \xc3\xa9", "UTF-8 split over transport reads") ||
        !check(term.takeInput() == "\x1b[1;8R", "device status reply")) return 1;
    term.feed("\x1b[?2004h");
    term.paste("a\r\nb\x1b[201~");
    if (!check(term.takeInput() == "\x1b[200~a\rb[201~\x1b[201~", "bracketed paste, CRLF and escape filtering")) return 1;
    if (!check(term.resize({10, 6}) && !term.resize({10, 6}), "resize change notification")) return 1;
    term.reset();
    if (!check(term.size().columns == 10 && term.size().rows == 6 && term.text().empty(), "reset keeps dimensions")) return 1;
    term.feed("01 3456789abcdefghijk");
    if (!check(term.text() == "01 3456789abcdefghijk", "soft-wrapped rows copy as one line")) return 1;
    term.reset();
    return keyboardTest(term) ? 0 : 1;
}
