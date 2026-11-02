# Web panels with CEF

CyUI's optional Web module embeds HTML/CSS/JavaScript interfaces in a Dear ImGui
panel. TypeScript is compiled to JavaScript by the web application's own build.
The supplied browser engine is **CEF**, using off-screen rendering into a texture
owned by the host renderer. This avoids a platform-native child window covering
ImGui popups or docking panels. No browser dependency enters Core.

## Platform and validation matrix

| Component | Windows | Linux | macOS |
| --- | --- | --- | --- |
| Core and ordinary C++ modules | Portable sources; MSVC build/tests/render checked locally | Portable sources; CI configured, not run locally | Portable sources; CI configured, not run locally |
| Python models / imgui-bundle UI | Models and real ImGui frames checked locally | Same Python sources; target environment needs validation | Same Python sources; target environment needs validation |
| Web policy, messages, JS/TS declarations | Tested | Platform-independent code | Platform-independent code |
| WebCEF off-screen adapter | Compiled; real page, paint and native/Python message round trips tested | Shared implementation; build/runtime packaging unverified here | Shared implementation; framework/helper/AppKit packaging unverified here |
| Win32/DX11 demo and BMP capture | Implemented and tested | Host supplies its ImGui backend | Host supplies its ImGui backend |

CEF itself supplies Windows, Linux and macOS distributions. CyUI does not claim
that adding a CMake option validates a finished application on all three systems.
CEF was tested here with `150.0.23+g4ccc4ca+chromium-150.0.7871.287`, Windows x64.
Choose and validate a maintained CEF SDK for each target architecture.

## Build selection

```cmake
set(CYUI_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(CYUI_MODULES "Web" CACHE STRING "" FORCE)
set(CYUI_BUILD_WEB_CEF ON CACHE BOOL "" FORCE)
set(CYUI_CEF_ROOT "path/to/extracted/cef_binary_distribution" CACHE PATH "" FORCE)
add_subdirectory(external/CyUI)
target_link_libraries(app PRIVATE CyUI::WebCEF)
```

Alternatively set `CYUI_CEF_TARGET` to a host target providing the CEF headers,
runtime library and C++ wrapper. This is useful when the application already owns
CEF packaging. The adapter requires C++20 for the tested SDK. Core remains C++17.
No SDK/runtime download happens during configure or drawing.

The build also creates `cyui_webcef_bridge` (a C ABI shared library) and
`cyui_webcef_helper` for optional Python/foreign hosts. A host already running CEF
should use its own process/app integration rather than initialize a second runtime.

## Native lifecycle

1. Pass `cyui::webCEFApp()` to both `CefExecuteProcess` and `CefInitialize` before
   creating views. Use the same renderer handler in all helper executables.
2. Set `CefSettings.windowless_rendering_enabled = true`. The application owns
   CEF initialization, sandbox policy, cache/profile directory and subprocess paths.
3. Supply an upload callback accepting an upper-left-origin BGRA8 buffer. Update
   a reusable host GPU texture and return its `ImTextureID`. The buffer expires
   after the callback. The texture remains host-owned, including final release.
4. On the CEF/UI thread call `CefDoMessageLoopWork()` regularly, `beginFrame()`,
   render `webPanel`, then `endFrame()`. The end call hides an undrawn tab/panel.
5. Call `close()` on all views and keep pumping until `closed()`. Destroy views
   before GPU resources, then call `CefShutdown()` on the initializing thread.

```cpp
#include <cyui/adapters/WebCEF.h>

cyui::WebCEFOptions options;
options.url = "http://127.0.0.1:4173/chat";
options.policy.origins = {"http://127.0.0.1:4173"};
options.upload = [&](int width, int height, const void* bgra) {
    return renderer.updateBrowserTexture(width, height, bgra);
};
auto web = cyui::createWebCEF(options); // after CefInitialize

// Each host frame, on the same thread:
CefDoMessageLoopWork();
web->beginFrame();
if (ImGui::Begin("Chat")) {
    for (const auto& event : cyui::webPanel("##chat", *web, {0, 0})) {
        if (event.kind == cyui::WebEventKind::navigated)
            web->postJson(cyui::webThemeJson());
        // Parse/validate application messages; apply host commands explicitly.
    }
}
ImGui::End();
web->endFrame();
```

The default software paint path bounds each view to 4096×4096 pixels and 30 fps;
options allow limits up to 8192. HTML select popups are composited into the same
texture. Native mouse/buttons/wheel, common keyboard keys and Unicode character
events are forwarded by webPanel. The host must keep platform text input enabled
while the backend is focused. Use `webCEFBrowser()` for platform-native key events,
IME composition/candidate placement, accessibility, external file drag/drop or
other advanced CEF integrations. These platform features are host responsibilities,
not validated by the basic pointer/text/message tests.

CEF message pumping is intentionally single-threaded here. Do not enable CEF's
multi-threaded message loop and then invoke this backend from a different ImGui
thread. Background engine work stays in CEF's subprocesses.

## Browser boundary

WebPolicy accepts exact HTTP(S) origins, including ports. It rejects credentials,
file/javascript URLs, wildcard matching, malformed ports and ambiguous backslashes.
Use a loopback server for a built local frontend or an explicitly permitted HTTPS
origin. CyUI does not start the frontend's server, compile TypeScript or choose its
authentication. CEF retains normal browser networking for page subresources.

Navigation outside allowed origins and new-window requests become `external`
events; the host decides how to handle them. Messages are accepted only from the
main frame of an allowed origin. JSON messages are limited to 1 MiB by default and
the event queue to 256 entries. Downloads, media permissions and other permission
prompts are denied by the supplied minimal adapter; enable product-specific
capabilities in an explicitly reviewed host integration if required.

CEF's process sandbox is a separate deployment concern. The library does not
silently turn it off. The isolated local browser tests explicitly set `no_sandbox`
to exercise the fixture SDK. A release application should configure and validate
the supported CEF sandbox for its platform. The Python initializer exposes an
explicit `no_sandbox=False` default; validate your helper and sandbox integration.

## Browser JavaScript / TypeScript

`web/` is the MIT `@cyui/web` package with JavaScript, TypeScript declarations and
an optional CSS stylesheet. CEF installs a narrow `window.cyuiNative` message
transport. No Electron or Node API is exposed to the page.

```ts
import {createCyUIBridge} from '@cyui/web';
const bridge = createCyUIBridge();
bridge.on('host.reply', payload => updateView(payload));
bridge.send('chat.message', {text: 'Hello'});
// Dispose listeners when the embedded UI is destroyed.
bridge.close();
```

Messages contain `{protocol: 'cyui.web', version: 1, type, payload, requestId?}`.
The application owns message schema, authorization and effects. A theme message
from webThemeJson()/theme_message() supplies palette CSS variables such as
`--cyui-bg0`, `--cyui-text` and `--cyui-accent`; send it again after a theme change.
JSON is transported as data through CEF process messages, without interpolating
host text as executable JavaScript.

## CyMD, CyAnnota and the other applications

`mountCyMD()` implements CyMD's existing version-1 iframe protocol: explicit
parent origin, frame identity and session checks, ready/open/save/state messages,
Blob documents and save acknowledgments. Serve a small host page and the built
CyMD frontend over HTTP(S), then mount it in that page. The host supplies onSave;
CyUI does not write files. Native JSON messages do not carry Blob objects directly:
the host page handles Blob/document transfer and can use a host document service
or an explicitly bounded encoded/chunked protocol.

CyAnnota and CyTask can be loaded as built web frontends with their required
services/adapters. Their Electron preload/IPC APIs must be replaced by explicit
host messages where needed. CyAIOrchestrator already has a web frontend. CyImgView
can host these web tools through a panel while using the native Images module.
No application is migrated or modified by this change. Each retains its own license.

## Python

```python
from cyui_modules.cef import CEFRuntime
from cyui_modules.web import web_panel, theme_message

with CEFRuntime(library_path, helper_path, absolute_cache_path) as runtime:
    view = runtime.create(url, (allowed_origin,), upload_bgra_to_host_texture)
    # In the host ImGui frame loop:
    runtime.begin_frame()
    events = web_panel('chat', view, (0, 400))
    for event in events:
        if event['kind'] == 'navigated':
            view.post_json(theme_message())
    runtime.end_frame()
```

All runtime/view calls must run on the owning UI thread. The context manager
closes and pumps all views before shutting CEF down. An upload callback receives
a native pointer valid only during the call; copy/update immediately. Python's
garbage collector does not initialize or shut down a browser on an arbitrary thread.

## Deployment and checks

- Windows: ship matching libcef/Chromium runtime files, resources/locales, helper
  executable and supported sandbox setup. The optional test/demo CMake staging
  copies the selected SDK's Release/Resources into the build output.
- Linux: ship/use matching libcef.so, resources/locales and helper; configure the
  runtime search path, target display stack and Chromium sandbox as CEF requires.
- macOS: package/load Chromium Embedded Framework.framework and correctly named
  signed helper app bundles, integrate CEF's AppKit application handling, entitlements
  and sandbox setup. The generic helper source is not a complete macOS app bundle.
- Keep the selected SDK's LICENSE.txt, CREDITS.html and other component notices.
  See [LICENSING.md](LICENSING.md).

```sh
cmake -S . -B build/cef -DCYUI_BUILD_DEMO=OFF -DCYUI_MODULES=Web \
  -DCYUI_BUILD_WEB_CEF=ON -DCYUI_CEF_ROOT=path/to/cef -DCYUI_BUILD_TESTS=ON
cmake --build build/cef --config Release
python -B tests/run_webcef.py build/cef/cyui_test_webcef.exe
```

Use the platform's executable/library paths for other systems. The local test
serves only its fixture routes, checks native and Python page/paint/message round
trips, and leaves profiles under ignored build output. The full CMake Win32 demo
also accepts `--web-url http://127.0.0.1:PORT/examples/web/index.html --tab 19` when
built with CEF. Its `--cef-no-sandbox` switch is explicit and intended for local
fixture validation; production hosts should provide their sandbox integration.

Reference: [CEF general usage](https://chromiumembedded.github.io/cef/general_usage.html),
[CEF binary distributions](https://cef-builds.spotifycdn.com/index.html).
