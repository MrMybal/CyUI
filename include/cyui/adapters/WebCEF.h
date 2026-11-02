#pragma once
#include "cyui/modules/Web.h"
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include <functional>
#include <memory>

namespace cyui {
struct WebCEFOptions {
    WebPolicy policy;
    CefWindowHandle parent{};
    std::string url;
    int maxWidth = 4096, maxHeight = 4096, framesPerSecond = 30;
    // BGRA8, upper-left origin. Called on CEF UI thread. Host owns/reuses/releases texture.
    std::function<ImTextureID(int width, int height, const void *bgra)> upload;
};
// Pass the SAME app to CefExecuteProcess and CefInitialize, including helper executables.
// Host owns CEF initialization, process sandbox, message pump and shutdown.
CefRefPtr<CefApp> webCEFApp();
std::unique_ptr<WebBackend> createWebCEF(const WebCEFOptions &);
// Advanced host access for IME composition, native key events, drag/drop and accessibility.
CefRefPtr<CefBrowser> webCEFBrowser(WebBackend &);
} // namespace cyui
