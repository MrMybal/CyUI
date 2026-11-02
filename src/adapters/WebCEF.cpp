#include "cyui/adapters/WebCEF.h"
#include "include/cef_client.h"
#include "include/cef_parser.h"
#include "include/cef_v8.h"
#include "include/wrapper/cef_helpers.h"
#include <cstring>
#include <deque>
#include <stdexcept>

namespace cyui {
namespace {
class PostHandler final : public CefV8Handler {
  public:
    bool Execute(const CefString &, CefRefPtr<CefV8Value>, const CefV8ValueList &args,
                 CefRefPtr<CefV8Value> &, CefString &exception) override {
        if (args.size() != 1 || !args[0]->IsString()) {
            exception = "Expected a JSON string";
            return true;
        }
        const auto text = args[0]->GetStringValue().ToString();
        if (text.size() > 1024 * 1024) {
            exception = "CyUI message exceeds 1 MiB";
            return true;
        }
        const auto context = CefV8Context::GetCurrentContext();
        if (!context || !context->GetFrame()->IsMain())
            return true;
        auto message = CefProcessMessage::Create("cyui.page");
        message->GetArgumentList()->SetString(0, text);
        context->GetFrame()->SendProcessMessage(PID_BROWSER, message);
        return true;
    }
    IMPLEMENT_REFCOUNTING(PostHandler);
};
class App final : public CefApp, public CefRenderProcessHandler {
  public:
    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override {
        return this;
    }
    void OnContextCreated(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame,
                          CefRefPtr<CefV8Context> context) override {
        if (!frame->IsMain())
            return;
        context->GetGlobal()->SetValue("__cyuiPost",
                                       CefV8Value::CreateFunction("__cyuiPost", new PostHandler),
                                       V8_PROPERTY_ATTRIBUTE_READONLY);
        frame->ExecuteJavaScript(
            R"JS((()=>{const listeners=new Set();Object.defineProperty(window,'cyuiNative',{value:Object.freeze({postMessage(message){window.__cyuiPost(JSON.stringify(message))},addEventListener(type,fn){if(type==='message')listeners.add(fn)},removeEventListener(type,fn){if(type==='message')listeners.delete(fn)}})});Object.defineProperty(window,'__cyuiReceive',{value(text){const data=JSON.parse(text);for(const fn of [...listeners])fn({data})}})})();)JS",
            frame->GetURL(), 0);
    }
    bool OnProcessMessageReceived(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, CefProcessId source,
                                  CefRefPtr<CefProcessMessage> message) override {
        if (source != PID_BROWSER || message->GetName() != "cyui.host" || !frame->IsMain())
            return false;
        const auto context = frame->GetV8Context();
        if (!context || !context->Enter())
            return true;
        auto function = context->GetGlobal()->GetValue("__cyuiReceive");
        if (function && function->IsFunction())
            function->ExecuteFunction(nullptr,
                                      {CefV8Value::CreateString(message->GetArgumentList()->GetString(0))});
        context->Exit();
        return true;
    }
    IMPLEMENT_REFCOUNTING(App);
};
struct State {
    WebCEFOptions options;
    WebPlacement placement;
    CefRefPtr<CefBrowser> browser;
    std::deque<WebEvent> events;
    int width = 1, height = 1, popupWidth = 0, popupHeight = 0;
    bool loaded = false, closing = false, closed = false, focused = false, popupVisible = false,
         placed = false;
    CefRect popup;
    std::vector<unsigned char> pixels, popupPixels, composite;
    ImTextureID texture = 0;
    std::string pending;
    void emit(WebEventKind kind, std::string url = {}, std::string data = {}) {
        if (data.size() > options.policy.maxMessageBytes || url.size() > 8192)
            return;
        if (events.size() >= std::max<std::size_t>(1, options.policy.maxQueuedEvents))
            events.pop_front();
        events.push_back({kind, std::move(url), std::move(data)});
    }
    void upload() {
        if (closing || pixels.empty() || !options.upload)
            return;
        const auto *data = pixels.data();
        if (popupVisible && !popupPixels.empty()) {
            composite = pixels;
            for (int y = 0; y < popupHeight; ++y) {
                const int destY = popup.y + y;
                if (destY < 0 || destY >= height)
                    continue;
                const int x = std::max(0, popup.x), end = std::min(width, popup.x + popupWidth);
                if (end > x)
                    std::memcpy(composite.data() + (std::size_t(destY) * width + x) * 4,
                                popupPixels.data() + (std::size_t(y) * popupWidth + x - popup.x) * 4,
                                std::size_t(end - x) * 4);
            }
            data = composite.data();
        }
        texture = options.upload(width, height, data);
    }
};
class Client final : public CefClient,
                     public CefRenderHandler,
                     public CefLifeSpanHandler,
                     public CefLoadHandler,
                     public CefRequestHandler,
                     public CefDisplayHandler,
                     public CefDownloadHandler,
                     public CefPermissionHandler,
                     public CefContextMenuHandler {
    std::shared_ptr<State> s_;

  public:
    explicit Client(std::shared_ptr<State> s) : s_(std::move(s)) {}
    CefRefPtr<CefRenderHandler> GetRenderHandler() override {
        return this;
    }
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
        return this;
    }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override {
        return this;
    }
    CefRefPtr<CefRequestHandler> GetRequestHandler() override {
        return this;
    }
    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override {
        return this;
    }
    CefRefPtr<CefDownloadHandler> GetDownloadHandler() override {
        return this;
    }
    CefRefPtr<CefPermissionHandler> GetPermissionHandler() override {
        return this;
    }
    CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() override {
        return this;
    }
    void GetViewRect(CefRefPtr<CefBrowser>, CefRect &rect) override {
        rect = {0, 0, s_->width, s_->height};
    }
    bool GetScreenInfo(CefRefPtr<CefBrowser>, CefScreenInfo &info) override {
        // Placement/input and the host texture use the same pixel coordinate space.
        // Host DPI is expressed through the CyUI CSS theme, not a second CEF scale.
        info.device_scale_factor = 1;
        info.depth = 32;
        info.depth_per_component = 8;
        info.rect = info.available_rect = {0, 0, s_->width, s_->height};
        return true;
    }
    bool GetScreenPoint(CefRefPtr<CefBrowser>, int x, int y, int &screenX, int &screenY) override {
        screenX = int(s_->placement.bounds.min.x) + x;
        screenY = int(s_->placement.bounds.min.y) + y;
        return true;
    }
    void OnPopupShow(CefRefPtr<CefBrowser>, bool show) override {
        s_->popupVisible = show;
        if (!show)
            s_->popupPixels.clear();
        s_->upload();
    }
    void OnPopupSize(CefRefPtr<CefBrowser>, const CefRect &rect) override {
        s_->popup = rect;
    }
    void OnPaint(CefRefPtr<CefBrowser>, PaintElementType type, const RectList &, const void *buffer,
                 int width, int height) override {
        CEF_REQUIRE_UI_THREAD();
        if (s_->closing || width < 1 || height < 1 || width > s_->options.maxWidth ||
            height > s_->options.maxHeight)
            return;
        const auto *bytes = static_cast<const unsigned char *>(buffer);
        const auto count = std::size_t(width) * height * 4;
        if (type == PET_VIEW) {
            if (width != s_->width || height != s_->height)
                return; // A queued paint from before the latest resize.
            s_->pixels.assign(bytes, bytes + count);
        } else {
            s_->popupWidth = width;
            s_->popupHeight = height;
            s_->popupPixels.assign(bytes, bytes + count);
        }
        s_->upload();
    }
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
        CEF_REQUIRE_UI_THREAD();
        s_->browser = browser;
        if (s_->closing) {
            browser->GetHost()->CloseBrowser(true);
            return;
        }
        browser->GetHost()->WasResized();
        browser->GetHost()->WasHidden(!s_->placement.visible);
        s_->emit(WebEventKind::ready);
        if (!s_->pending.empty())
            browser->GetMainFrame()->LoadURL(s_->pending);
    }
    void OnBeforeClose(CefRefPtr<CefBrowser>) override {
        CEF_REQUIRE_UI_THREAD();
        s_->browser = nullptr;
        s_->closed = true;
        s_->texture = 0;
    }
    bool OnBeforePopup(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, int, const CefString &url,
                       const CefString &, cef_window_open_disposition_t, bool, const CefPopupFeatures &,
                       CefWindowInfo &, CefRefPtr<CefClient> &, CefBrowserSettings &,
                       CefRefPtr<CefDictionaryValue> &, bool *) override {
        s_->emit(WebEventKind::external, url);
        return true;
    }
    bool OnBeforeBrowse(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest> request, bool,
                        bool) override {
        if (!frame->IsMain())
            return false;
        const auto url = request->GetURL().ToString();
        if (url == "about:blank")
            return false;
        s_->loaded = false;
        if (!s_->options.policy.allows(url)) {
            s_->emit(WebEventKind::external, url);
            return true;
        }
        return false;
    }
    void OnLoadEnd(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, int status) override {
        if (!frame->IsMain())
            return;
        const auto url = frame->GetURL().ToString();
        s_->loaded = s_->options.policy.allows(url);
        if (s_->loaded)
            s_->emit(WebEventKind::navigated, url, std::to_string(status));
    }
    void OnLoadError(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, ErrorCode code, const CefString &text,
                     const CefString &url) override {
        if (frame->IsMain() && code != ERR_ABORTED) {
            s_->loaded = false;
            s_->emit(WebEventKind::error, url, text);
        }
    }
    bool OnConsoleMessage(CefRefPtr<CefBrowser>, cef_log_severity_t level, const CefString &message,
                          const CefString &source, int) override {
        if (level >= LOGSEVERITY_ERROR)
            s_->emit(WebEventKind::error, source, message);
        return false;
    }
    bool OnProcessMessageReceived(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, CefProcessId source,
                                  CefRefPtr<CefProcessMessage> message) override {
        if (source != PID_RENDERER || message->GetName() != "cyui.page")
            return false;
        const auto url = frame->GetURL().ToString(),
                   text = message->GetArgumentList()->GetString(0).ToString();
        if (frame->IsMain() && s_->options.policy.allows(url) &&
            text.size() <= s_->options.policy.maxMessageBytes && CefParseJSON(text, JSON_PARSER_RFC))
            s_->emit(WebEventKind::message, url, text);
        return true;
    }
    void OnBeforeContextMenu(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefContextMenuParams>,
                             CefRefPtr<CefMenuModel> model) override {
        model->Clear();
    }
    bool OnBeforeDownload(CefRefPtr<CefBrowser>, CefRefPtr<CefDownloadItem>, const CefString &,
                          CefRefPtr<CefBeforeDownloadCallback>) override {
        return false;
    }
    bool OnRequestMediaAccessPermission(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, const CefString &,
                                        uint32_t, CefRefPtr<CefMediaAccessCallback> callback) override {
        callback->Cancel();
        return true;
    }
    bool OnShowPermissionPrompt(CefRefPtr<CefBrowser>, uint64_t, const CefString &, uint32_t,
                                CefRefPtr<CefPermissionPromptCallback> callback) override {
        callback->Continue(CEF_PERMISSION_RESULT_DENY);
        return true;
    }
    IMPLEMENT_REFCOUNTING(Client);
};
uint32_t modifiers(unsigned value) {
    uint32_t out = 0;
    if (value & 1)
        out |= EVENTFLAG_SHIFT_DOWN;
    if (value & 2)
        out |= EVENTFLAG_CONTROL_DOWN;
    if (value & 4)
        out |= EVENTFLAG_ALT_DOWN;
    if (value & 8)
        out |= EVENTFLAG_COMMAND_DOWN;
    if (value & 16)
        out |= EVENTFLAG_LEFT_MOUSE_BUTTON;
    if (value & 32)
        out |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
    if (value & 64)
        out |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
    return out;
}
class Backend final : public WebBackend {
    std::shared_ptr<State> s_;

  public:
    explicit Backend(const WebCEFOptions &o) : s_(std::make_shared<State>()) {
        CEF_REQUIRE_UI_THREAD();
        if (!o.upload)
            throw std::invalid_argument("WebCEF requires a host BGRA texture upload callback");
        s_->options = o;
        s_->options.maxWidth = std::clamp(o.maxWidth, 1, 8192);
        s_->options.maxHeight = std::clamp(o.maxHeight, 1, 8192);
        s_->placement.visible = false;
        if (!o.url.empty() && !o.policy.allows(o.url))
            throw std::invalid_argument("WebCEF URL is outside the allowed origins");
        s_->pending = o.url;
        CefWindowInfo window;
        window.SetAsWindowless(o.parent);
        window.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
        CefBrowserSettings settings;
        settings.windowless_frame_rate = std::clamp(o.framesPerSecond, 1, 60);
        settings.background_color = 0;
        if (!CefBrowserHost::CreateBrowser(window, new Client(s_), "about:blank", settings, nullptr,
                                           nullptr)) {
            s_->closed = true;
            throw std::runtime_error("CEF browser creation failed");
        }
    }
    ~Backend() override {
        close();
    }
    void close() override {
        CEF_REQUIRE_UI_THREAD();
        if (s_->closing)
            return;
        s_->closing = true;
        s_->options.upload = {};
        if (s_->browser)
            s_->browser->GetHost()->CloseBrowser(true);
    }
    bool closed() const override {
        return s_->closed;
    }
    void beginFrame() override {
        s_->placed = false;
    }
    void endFrame() override {
        if (!s_->placed && s_->placement.visible) {
            auto p = s_->placement;
            p.visible = false;
            place(p);
            input({WebInputKind::focus});
        }
    }
    void place(const WebPlacement &p) override {
        CEF_REQUIRE_UI_THREAD();
        s_->placed = true;
        const int width = std::clamp(int(std::ceil(p.bounds.size().x)), 1, s_->options.maxWidth),
                  height = std::clamp(int(std::ceil(p.bounds.size().y)), 1, s_->options.maxHeight);
        const bool resized = width != s_->width || height != s_->height,
                   visibility = p.visible != s_->placement.visible;
        s_->placement = p;
        s_->width = width;
        s_->height = height;
        if (s_->browser && !s_->closing) {
            if (resized)
                s_->browser->GetHost()->WasResized();
            if (visibility)
                s_->browser->GetHost()->WasHidden(!p.visible);
            if (p.focus) {
                s_->focused = true;
                s_->browser->GetHost()->SetFocus(true);
            }
        }
    }
    bool navigate(const std::string &url) override {
        CEF_REQUIRE_UI_THREAD();
        if (s_->closing || !s_->options.policy.allows(url)) {
            s_->emit(WebEventKind::external, url);
            return false;
        }
        s_->loaded = false;
        s_->pending = url;
        if (s_->browser)
            s_->browser->GetMainFrame()->LoadURL(url);
        return true;
    }
    bool postJson(const std::string &json) override {
        CEF_REQUIRE_UI_THREAD();
        if (s_->closing || !s_->browser || !s_->loaded || json.size() > s_->options.policy.maxMessageBytes ||
            !CefParseJSON(json, JSON_PARSER_RFC))
            return false;
        auto message = CefProcessMessage::Create("cyui.host");
        message->GetArgumentList()->SetString(0, json);
        s_->browser->GetMainFrame()->SendProcessMessage(PID_RENDERER, message);
        return true;
    }
    bool poll(WebEvent &event) override {
        CEF_REQUIRE_UI_THREAD();
        if (s_->events.empty())
            return false;
        event = std::move(s_->events.front());
        s_->events.pop_front();
        return true;
    }
    bool ready() const override {
        return bool(s_->browser) && !s_->closing;
    }
    bool usesTexture() const override {
        return true;
    }
    ImTextureID texture() const override {
        return s_->texture;
    }
    bool focused() const override {
        return s_->focused;
    }
    CefRefPtr<CefBrowser> browser() const {
        return s_->browser;
    }
    void input(const WebInput &e) override {
        CEF_REQUIRE_UI_THREAD();
        if (!s_->browser || s_->closing)
            return;
        auto host = s_->browser->GetHost();
        CefMouseEvent mouse;
        mouse.x = int(e.x * s_->width / std::max(1.f, s_->placement.bounds.size().x));
        mouse.y = int(e.y * s_->height / std::max(1.f, s_->placement.bounds.size().y));
        mouse.modifiers = modifiers(e.modifiers);
        switch (e.kind) {
        case WebInputKind::focus:
            s_->focused = e.down;
            host->SetFocus(e.down);
            break;
        case WebInputKind::move:
            host->SendMouseMoveEvent(mouse, !e.down);
            break;
        case WebInputKind::button:
            host->SendMouseClickEvent(mouse,
                                      e.button == 0   ? MBT_LEFT
                                      : e.button == 1 ? MBT_RIGHT
                                                      : MBT_MIDDLE,
                                      !e.down, e.clicks);
            break;
        case WebInputKind::wheel:
            host->SendMouseWheelEvent(mouse, int(e.wheelX * 120), int(e.wheelY * 120));
            break;
        case WebInputKind::key: {
            if (s_->focused)
                host->SetFocus(true);
            CefKeyEvent key;
            key.type = e.down ? KEYEVENT_RAWKEYDOWN : KEYEVENT_KEYUP;
            key.windows_key_code = e.key;
            key.native_key_code = e.nativeKey;
            key.modifiers = mouse.modifiers;
            host->SendKeyEvent(key);
            break;
        }
        case WebInputKind::text: {
            if (s_->focused)
                host->SetFocus(true);
            unsigned cp = e.codepoint;
            if (cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
                break;
            CefKeyEvent key;
            key.type = KEYEVENT_CHAR;
            key.modifiers = mouse.modifiers;
            if (cp > 0xffff) {
                cp -= 0x10000;
                key.windows_key_code = key.character = key.unmodified_character =
                    char16_t(0xd800 + (cp >> 10));
                host->SendKeyEvent(key);
                cp = 0xdc00 + (cp & 1023);
            }
            key.windows_key_code = key.character = key.unmodified_character = char16_t(cp);
            host->SendKeyEvent(key);
            break;
        }
        }
    }
};
} // namespace
CefRefPtr<CefApp> webCEFApp() {
    return new App;
}
std::unique_ptr<WebBackend> createWebCEF(const WebCEFOptions &o) {
    return std::make_unique<Backend>(o);
}
CefRefPtr<CefBrowser> webCEFBrowser(WebBackend &backend) {
    auto *cef = dynamic_cast<Backend *>(&backend);
    return cef ? cef->browser() : nullptr;
}
} // namespace cyui
