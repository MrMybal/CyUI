#pragma once
#include "cyui/Core.h"
#include "cyui/Interaction.h"
#include <string>

namespace cyui {
// Exact http(s) origins only. No file:, javascript:, credentials or wildcard hosts.
std::string webOrigin(const std::string &url);
struct WebPolicy {
    std::vector<std::string> origins;
    std::size_t maxMessageBytes = 1024 * 1024, maxQueuedEvents = 256;
    bool allows(const std::string &url) const;
};
enum class WebEventKind { ready, navigated, message, external, error };
struct WebEvent {
    WebEventKind kind = WebEventKind::error;
    std::string url, data;
};
struct WebPlacement {
    Rect bounds, clip;
    bool visible = true, focus = false;
};
enum class WebInputKind { move, button, wheel, key, text, focus };
struct WebInput {
    WebInputKind kind = WebInputKind::move;
    float x = 0, y = 0, wheelX = 0, wheelY = 0;
    int button = 0, clicks = 1, key = 0, nativeKey = 0;
    unsigned modifiers = 0, codepoint = 0;
    bool down = false;
};
// All methods run on the host UI thread. No renderer/browser is loaded by Core.
class WebBackend {
  public:
    virtual ~WebBackend() = default;
    virtual void place(const WebPlacement &) = 0;
    virtual bool navigate(const std::string &) = 0;
    virtual bool postJson(const std::string &) = 0;
    virtual bool poll(WebEvent &) = 0;
    virtual bool ready() const = 0;
    virtual ImTextureID texture() const {
        return 0;
    }
    virtual bool usesTexture() const {
        return false;
    }
    virtual void input(const WebInput &) {}
    virtual bool focused() const {
        return false;
    }
    virtual void close() {}
    virtual bool closed() const {
        return true;
    }
    virtual void beginFrame() {}
    virtual void endFrame() {}
};
struct WebPanelOptions {
    bool visible = true, focus = false;
    ImVec2 viewportOrigin{};
};
// Call backend.place({..., ..., false}) at frame start, including hidden tabs.
// Bounds are host-client pixels after subtracting viewportOrigin.
std::vector<WebEvent> webPanel(const char *id, WebBackend &, ImVec2 size, const WebPanelOptions & = {});
std::string webThemeJson();
std::string webEventJson(const WebEvent &);
} // namespace cyui
