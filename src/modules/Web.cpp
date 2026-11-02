#include "cyui/modules/Web.h"
#include <cctype>
#include <sstream>
#include <locale>

namespace cyui {
namespace {
std::string quote(const std::string &value) {
    std::string out = "\"";
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += char(c);
        } else if (c < 32) {
            const char *hex = "0123456789abcdef";
            out += "\\u00";
            out += hex[c >> 4];
            out += hex[c & 15];
        } else
            out += char(c);
    }
    return out + '"';
}
} // namespace
std::string webOrigin(const std::string &url) {
    if (url.size() > 8192 || url.find_first_of("\\\r\n\t ") != std::string::npos)
        return {};
    for (unsigned char c : url)
        if (c < 32 || c == 127)
            return {};
    const auto marker = url.find("://");
    if (marker == std::string::npos)
        return {};
    auto scheme = url.substr(0, marker);
    for (auto &c : scheme)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    if (scheme != "http" && scheme != "https")
        return {};
    const auto end = url.find_first_of("/?#", marker + 3);
    auto authority = url.substr(marker + 3, end == std::string::npos ? end : end - marker - 3);
    if (authority.empty() || authority.find('@') != std::string::npos ||
        authority.find('%') != std::string::npos)
        return {};
    for (auto &c : authority) {
        if (static_cast<unsigned char>(c) >= 128)
            return {};
        c = char(std::tolower(static_cast<unsigned char>(c)));
    }
    std::string host, port;
    if (authority.front() == '[') {
        const auto close = authority.find(']');
        if (close == std::string::npos || close <= 2)
            return {};
        host = authority.substr(0, close + 1);
        for (std::size_t n = 1; n < close; ++n)
            if (!std::isxdigit(static_cast<unsigned char>(host[n])) && host[n] != ':')
                return {};
        if (close + 1 < authority.size()) {
            if (authority[close + 1] != ':')
                return {};
            port = authority.substr(close + 2);
            if (port.empty())
                return {};
        }
    } else {
        const auto colon = authority.find(':');
        host = authority.substr(0, colon);
        if (colon != std::string::npos) {
            port = authority.substr(colon + 1);
            if (port.empty())
                return {};
        }
        for (char c : host)
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '-')
                return {};
    }
    if (host.empty())
        return {};
    if (!port.empty()) {
        unsigned number = 0;
        for (char c : port) {
            if (c < '0' || c > '9')
                return {};
            number = number * 10 + unsigned(c - '0');
            if (number > 65535)
                return {};
        }
        if (number == 0)
            return {};
        port = std::to_string(number);
        if ((scheme == "https" && port == "443") || (scheme == "http" && port == "80"))
            port.clear();
    }
    return scheme + "://" + host + (port.empty() ? "" : ":" + port);
}
bool WebPolicy::allows(const std::string &url) const {
    const auto origin = webOrigin(url);
    if (origin.empty())
        return false;
    for (const auto &entry : origins)
        if (webOrigin(entry) == origin)
            return true;
    return false;
}
std::vector<WebEvent> webPanel(const char *id, WebBackend &backend, ImVec2 size, const WebPanelOptions &o) {
    if (size.x <= 0)
        size.x = std::max(dp(1), ImGui::GetContentRegionAvail().x + size.x);
    if (size.y <= 0)
        size.y = std::max(dp(1), ImGui::GetContentRegionAvail().y + size.y);
    const auto a = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, size);
    const auto b = ImVec2(a.x + size.x, a.y + size.y);
    auto *d = ImGui::GetWindowDrawList();
    const auto low = d->GetClipRectMin(), high = d->GetClipRectMax();
    WebPlacement placement;
    placement.bounds = {{a.x - o.viewportOrigin.x, a.y - o.viewportOrigin.y},
                        {b.x - o.viewportOrigin.x, b.y - o.viewportOrigin.y}};
    placement.clip = {
        {std::max(a.x, low.x) - o.viewportOrigin.x, std::max(a.y, low.y) - o.viewportOrigin.y},
        {std::min(b.x, high.x) - o.viewportOrigin.x, std::min(b.y, high.y) - o.viewportOrigin.y}};
    placement.visible =
        o.visible && ImGui::IsItemVisible() && placement.clip.size().x > 0 && placement.clip.size().y > 0;
    placement.focus = o.focus;
    d->AddRectFilled(a, b, palette().bg0);
    if (!backend.ready())
        centredText(d, a, b, palette().textDim, "Web view loading...");
    backend.place(placement);
    if (backend.texture())
        d->AddImage(backend.texture(), a, b);
    if (backend.usesTexture() && placement.visible) {
        auto &io = ImGui::GetIO();
        const bool hovered = ImGui::IsItemHovered();
        WebInput event;
        event.x = io.MousePos.x - a.x;
        event.y = io.MousePos.y - a.y;
        event.modifiers = (io.KeyShift ? 1u : 0u) | (io.KeyCtrl ? 2u : 0u) | (io.KeyAlt ? 4u : 0u) |
                          (io.KeySuper ? 8u : 0u) | (io.MouseDown[0] ? 16u : 0u) |
                          (io.MouseDown[1] ? 32u : 0u) | (io.MouseDown[2] ? 64u : 0u);
        if (ImGui::IsMouseClicked(0)) {
            event.kind = WebInputKind::focus;
            event.down = hovered;
            backend.input(event);
        }
        event.kind = WebInputKind::move;
        event.down = hovered;
        backend.input(event);
        for (int button = 0; button < 3; ++button)
            if ((hovered || backend.focused()) &&
                (ImGui::IsMouseClicked(button) || ImGui::IsMouseReleased(button))) {
                event.kind = WebInputKind::button;
                event.button = button;
                event.down = ImGui::IsMouseClicked(button);
                event.clicks = ImGui::IsMouseDoubleClicked(button) ? 2 : 1;
                backend.input(event);
            }
        if (hovered && (io.MouseWheel || io.MouseWheelH) && !wheelTaken()) {
            event.kind = WebInputKind::wheel;
            event.wheelX = io.MouseWheelH;
            event.wheelY = io.MouseWheel;
            backend.input(event);
            takeWheel();
        }
        if (backend.focused()) {
            ImGui::SetNextFrameWantCaptureKeyboard(true);
            event.kind = WebInputKind::key;
            auto key = [&](ImGuiKey code, int virtualKey) {
                if (ImGui::IsKeyPressed(code) || ImGui::IsKeyReleased(code)) {
                    event.key = virtualKey;
                    event.down = ImGui::IsKeyDown(code);
                    backend.input(event);
                }
            };
            for (int n = 0; n < 26; ++n)
                key(ImGuiKey(ImGuiKey_A + n), 0x41 + n);
            for (int n = 0; n < 10; ++n)
                key(ImGuiKey(ImGuiKey_0 + n), 0x30 + n);
            key(ImGuiKey_Backspace, 8);
            key(ImGuiKey_Tab, 9);
            key(ImGuiKey_Enter, 13);
            key(ImGuiKey_Escape, 27);
            key(ImGuiKey_Space, 32);
            key(ImGuiKey_PageUp, 33);
            key(ImGuiKey_PageDown, 34);
            key(ImGuiKey_End, 35);
            key(ImGuiKey_Home, 36);
            key(ImGuiKey_LeftArrow, 37);
            key(ImGuiKey_UpArrow, 38);
            key(ImGuiKey_RightArrow, 39);
            key(ImGuiKey_DownArrow, 40);
            key(ImGuiKey_Insert, 45);
            key(ImGuiKey_Delete, 46);
            event.kind = WebInputKind::text;
            for (auto cp : io.InputQueueCharacters) {
                event.codepoint = unsigned(cp);
                backend.input(event);
            }
        }
    }
    std::vector<WebEvent> events;
    WebEvent event;
    while (events.size() < 256 && backend.poll(event))
        events.push_back(std::move(event));
    return events;
}
std::string webThemeJson() {
    const auto &p = palette();
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << "{\"protocol\":\"cyui.web\",\"version\":1,\"type\":\"theme\",\"payload\":{\"scale\":" << dp(1)
           << ",\"colors\":{";
    bool first = true;
    auto color = [&](const char *name, ImU32 value) {
        if (!first)
            stream << ',';
        first = false;
        const auto v = vec(value);
        stream << quote(name) << ":\"rgba(" << int(v.x * 255 + .5f) << ',' << int(v.y * 255 + .5f) << ','
               << int(v.z * 255 + .5f) << ',' << v.w << ")\"";
    };
    color("bg0", p.bg0);
    color("bg1", p.bg1);
    color("bg2", p.bg2);
    color("bg3", p.bg3);
    color("text", p.text);
    color("textDim", p.textDim);
    color("textFaint", p.textFaint);
    color("accent", p.accent);
    color("selection", p.selection);
    color("line", p.line);
    color("success", p.success);
    color("warning", p.warning);
    color("error", p.error);
    color("info", p.info);
    stream << "}}}";
    return stream.str();
}
std::string webEventJson(const WebEvent &e) {
    const char *kind = e.kind == WebEventKind::ready       ? "ready"
                       : e.kind == WebEventKind::navigated ? "navigated"
                       : e.kind == WebEventKind::message   ? "message"
                       : e.kind == WebEventKind::external  ? "external"
                                                           : "error";
    return "{\"kind\":" + quote(kind) + ",\"url\":" + quote(e.url) + ",\"data\":" + quote(e.data) + "}";
}
} // namespace cyui
