#include "cyui/adapters/WebCEF.h"
#include "include/cef_parser.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>
#if defined(OS_WIN)
#include <windows.h>
#endif

int main(int argc, char **argv) {
#if defined(OS_WIN)
    CefMainArgs args(GetModuleHandleW(nullptr));
#else
    CefMainArgs args(argc, argv);
#endif
    auto app = cyui::webCEFApp();
    const int child = CefExecuteProcess(args, app, nullptr);
    if (child >= 0)
        return child;
    if (argc != 3) {
        std::fprintf(stderr, "Usage: cyui_test_webcef http://127.0.0.1:PORT/examples/web/index.html "
                             "absolute-cache-directory\n");
        return 2;
    }
    CefSettings settings;
    settings.windowless_rendering_enabled = true;
    settings.no_sandbox = true;
    settings.log_severity = LOGSEVERITY_WARNING;
    CefString(&settings.root_cache_path) = std::filesystem::absolute(argv[2]).string();
    if (!CefInitialize(args, settings, app, nullptr))
        return 3;
#if defined(OS_WIN)
    const auto parent =
        CreateWindowExW(WS_EX_TOOLWINDOW, L"STATIC", L"CyUI CEF fixture", WS_OVERLAPPEDWINDOW, -32000, -32000,
                        800, 600, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(parent, SW_SHOWNOACTIVATE);
#endif
    int result = 1;
    {
        std::size_t frames = 0;
        cyui::WebCEFOptions options;
        options.policy.origins = {cyui::webOrigin(argv[1])};
        options.url = argv[1];
        options.upload = [&](int w, int h, const void *data) {
            if (w > 1 && h > 1 && data)
                ++frames;
            return ImTextureID(1);
        };

#if defined(OS_WIN)
        options.parent = parent;
#endif
        auto web = cyui::createWebCEF(options);
        web->place({{{0, 0}, {800, 600}}, {{0, 0}, {800, 600}}, true, false});
        if (web->navigate("https://example.invalid/") || web->navigate("file:///private.txt"))
            return 4;
        bool page = false, reply = false, typed = false, sentInput = false, focused = false,
             sentTyping = false, pointer = false, doubleClick = false, keyCode = false;
#if !defined(OS_WIN)
        keyCode = true; // Native scan codes are synthesized on Windows only.
#endif
        float inputX = 100, inputY = 100;
        const auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(25) &&
               (!reply || !frames || !typed || !doubleClick || !keyCode)) {
#if defined(OS_WIN)
            MSG nativeMessage;
            while (PeekMessageW(&nativeMessage, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&nativeMessage);
                DispatchMessageW(&nativeMessage);
            }
#endif
            CefDoMessageLoopWork();
            cyui::WebEvent event;
            while (web->poll(event)) {
                if (event.kind == cyui::WebEventKind::error)
                    std::fprintf(stderr, "CEF: %s\n", event.data.c_str());
                if (event.kind != cyui::WebEventKind::message)
                    continue;
                if (event.data.find("page.ready") != std::string::npos) {
                    page = true;
                    const auto value = CefParseJSON(event.data, JSON_PARSER_RFC);
                    const auto dictionary = value ? value->GetDictionary() : nullptr;
                    const auto payload = dictionary ? dictionary->GetDictionary("payload") : nullptr;
                    const auto input = payload ? payload->GetDictionary("input") : nullptr;
                    if (input) {
                        auto number = [&](const char *key) {
                            return input->GetType(key) == VTYPE_INT ? double(input->GetInt(key))
                                                                    : input->GetDouble(key);
                        };
                        inputX = float(number("x"));
                        inputY = float(number("y"));
                    }
                }
                reply = reply || event.data.find("test.reply") != std::string::npos;
                focused = focused || event.data.find("test.focus") != std::string::npos;
                pointer = pointer || event.data.find("test.pointer.any") != std::string::npos;
                doubleClick = doubleClick || event.data.find("test.dblclick") != std::string::npos;
                // Windows: the adapter supplies the layout's scan code, so the page sees
                // KeyboardEvent.code (KeyA on QWERTY, KeyQ on AZERTY).
                keyCode = keyCode || (event.data.find("test.key") != std::string::npos &&
                                      event.data.find("\"code\":\"Key") != std::string::npos);
                typed = typed || (event.data.find("test.input") != std::string::npos &&
                                  event.data.find("\xe9\x9f\xb3\xe6\xa5\xbd") != std::string::npos);
            }
            if (page && web->postJson("{\"protocol\":\"cyui.web\",\"version\":1,\"type\":\"host.reply\","
                                      "\"payload\":{\"ok\":true}}"))
                page = false;
            if (reply && !sentInput && std::chrono::steady_clock::now() - start > std::chrono::seconds(2)) {
                cyui::WebInput input;
                input.kind = cyui::WebInputKind::focus;
                input.down = true;
                web->input(input);
                input.x = inputX;
                input.y = inputY;
                input.kind = cyui::WebInputKind::move;
                web->input(input);
                input.kind = cyui::WebInputKind::button;
                input.down = true;
                input.modifiers = 16;
                web->input(input);
                input.down = false;
                input.modifiers = 0;
                web->input(input);
                // Second click: the release says clicks=1, the adapter reuses the press's 2.
                input.down = true;
                input.clicks = 2;
                input.modifiers = 16;
                web->input(input);
                input.down = false;
                input.clicks = 1;
                input.modifiers = 0;
                web->input(input);
                sentInput = true;
            }
            if (focused && sentInput && !sentTyping &&
                std::chrono::steady_clock::now() - start > std::chrono::seconds(3)) {
                cyui::WebInput input;
                input.kind = cyui::WebInputKind::key;
                input.key = 65;
                input.modifiers = 2;
                input.down = true;
                web->input(input);
                input.down = false;
                web->input(input);
                input.modifiers = 0;
                input.kind = cyui::WebInputKind::text;
                input.codepoint = 0x97f3;
                web->input(input);
                input.codepoint = 0x697d;
                web->input(input);
                sentTyping = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        std::fprintf(stderr, "reply=%d frames=%zu typed=%d pointer=%d dblclick=%d code=%d\n", reply, frames,
                     typed, pointer, doubleClick, keyCode);
        if (reply && frames && typed && pointer && doubleClick && keyCode) {
            std::ofstream(std::filesystem::path(argv[2]) / "cyui-result.txt")
                << "painted=" << frames << ";messages=bidirectional;input=unicode;dblclick;code";
            std::printf("CEF real page, %zu painted frames, messages and Unicode input passed\n", frames);
            result = 0;
        }
        web->close();
        const auto closing = std::chrono::steady_clock::now();
        while (!web->closed() && std::chrono::steady_clock::now() - closing < std::chrono::seconds(5)) {
            CefDoMessageLoopWork();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if (!web->closed())
            result = 5;
    }
#if defined(OS_WIN)
    DestroyWindow(parent);
#endif
    CefShutdown();
    return result;
}
