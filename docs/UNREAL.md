# CyUI inside Unreal Editor

The optional `CyUI` Unreal plugin draws CyUI tools in dockable editor tabs or
embedded Slate panels. It provides its own Dear ImGui 1.92 renderer/platform
backend using Slate custom vertices, clipping, dynamic font textures, Unreal
textures, clipboard, focus, mouse capture, keyboard navigation and Unicode input.
It does not require another Unreal ImGui plugin or a browser engine.

This is an **Editor** module, disabled by default. Core/CMake/Python consumers do
not acquire Unreal dependencies. It is intended for native C++ editor tools,
not UMG, runtime gameplay UI, Blueprints or remote rendering.

## Prepare and install

Use Python 3.10+ from the CyUI checkout. Choose a **new** output directory:

```sh
python tools/package_unreal.py --output build/UnrealPlugin/CyUI --modules Core Plots Inspector Code Diff --gallery
```

Copy the resulting `CyUI` directory to `<YourProject>/Plugins/CyUI`, enable
**CyUI Editor Tools**, and build the project's Editor target with UnrealBuildTool
or the IDE. Open **Window → CyUI → CyUI Tools**. With `--gallery`, the Window menu
also offers **CyUI Gallery** and examples of the selected optional modules.

`--modules Core` is minimal. `--modules All` includes all ordinary CyUI modules.
Dependencies are added automatically: Diff includes Code, Curves includes Plots
and Canvas, and Viewport includes Images and Canvas. Audio/Gallery are explicit
choices. CEF and the external ImGuizmo adapter are not included by this script.

The preparer copies a self-contained source snapshot and original MIT dependency
licenses, with generated UBT source bridges. The library stays under the plugin's
`ThirdParty/CyUI`, outside UBT's automatic source discovery to avoid double
compilation. No engine source is copied. UBT compiles C++20 with exceptions and
exports the same CyUI/ImGui instance to consumer modules. `CYUI_API` defaults to
empty outside Unreal, preserving static CMake integration.

Existing output directories are refused. To update an installed plugin, prepare
a new staging directory, close the editor, and replace the old plugin deliberately.
Keep tool-specific code in your own Editor module. The connector disables dynamic
module unloading; restart the editor after changing its backend/library sources.

## Register an editor tool

In your Editor module's `.Build.cs`:

```csharp
PrivateDependencyModuleNames.AddRange(new[] { "Core", "CyUI" });
CppStandard = CppStandardVersion.Cpp20;
```

Register the tool in `StartupModule`, unregister it in `ShutdownModule`:

```cpp
#include "CyUIEditorModule.h"
#include <cyui/Core.h>

void FMyEditorTools::StartupModule()
{
    FCyUIEditorModule::Get().RegisterTool(
        "MyTools.Inspector", NSLOCTEXT("MyTools", "Inspector", "My CyUI Inspector"),
        FOnDrawCyUITool::CreateRaw(this, &FMyEditorTools::DrawInspector));
}

void FMyEditorTools::DrawInspector()
{
    cyui::heading("My editor tool");
    if (cyui::primaryButton("Inspect selection")) {
        // Read the editor selection / request a host action here.
    }
}

void FMyEditorTools::ShutdownModule()
{
    if (FModuleManager::Get().IsModuleLoaded("CyUI"))
        FCyUIEditorModule::Get().UnregisterTool("MyTools.Inspector");
}
```

`RegisterTool` rejects empty/duplicate IDs and unbound callbacks. `OpenTool(id)`
opens the registered nomad tab. Unregistration detaches the callback before
requesting tab closure, so a closed/unloading owner cannot keep drawing.
For an existing Slate layout, `MakePanel(callback)` returns an `SWidget`.
Keep the owning callback valid for its panel's lifetime; cast to `SCyUIPanel` and
call `Detach()` before destroying a raw callback owner.

Both methods accept an optional `FOnInitializeCyUITool` as their last argument.
It runs once per newly created panel with its context current, after the default
fonts are loaded and before the first frame. Use it to merge a host-supplied font
into `cyui::uiFont()` (set `ImFontConfig::MergeMode` and `DstFont`) or configure
additional fonts. `SCyUIPanel` exposes the same `OnInitialize` Slate argument.
Unicode storage/input does not supply missing font glyphs: choose fonts covering
the scripts your tool displays, with appropriate distribution rights.

The connector owns `CreateContext`, `NewFrame`, the root `Begin/End`, `Render` and
notifications. The callback draws **contents**. Do not run another ImGui frame,
destroy/change the context or load fonts from a draw callback. CyUI Gallery may
create its own ordinary ImGui windows inside the panel. Native OS windows and
docking belong to Slate; ImGui multi-viewport OS windows are disabled.

All APIs/callbacks run on the editor game thread. Work that changes assets or the
world must use the owning Unreal tool's transactions, `Modify`, validation and
Undo/Redo. Draw callbacks may run more than once per editor frame; execute actions
on widget events rather than unconditionally. Read/prepare expensive data outside
the paint callback.

Each panel owns an independent ImGui context. Numeric-entry buffers, wheel state
and notifications are scoped to that context and released on context shutdown.
Palette and density are shared by CyUI's module instance, so changing the theme
in one tool styles the other panels on their next paint. Slate handles desktop
DPI, while `DisplayFramebufferScale` supplies font rasterization density. Keep
CyUI's logical DPI at 1 within this host.

## Show Unreal textures

```cpp
const uint64 imageId = FCyUIEditorModule::Get().RegisterTexture(myTexture);
// In the draw callback (or ImageData for CyUI::Images / Viewport):
ImGui::Image(ImTextureRef(ImTextureID(imageId)), cyui::dp(256, 256));
// After the tool stops submitting this image:
FCyUIEditorModule::Get().UnregisterTexture(imageId);
```

Registration holds a strong UObject reference. Unregistration finishes submitted
Slate/render commands before releasing that reference; avoid doing it every paint.
Register/unregister textures outside draw callbacks, after stopping submissions
of the old image, for example in the owning tool's setup/teardown.
ID 0 and stale/unregistered IDs are skipped. Texture IDs from other ImGui or RHI
backends are not valid here. Only ordinary Slate-renderable `UTexture` resources
are supported by this registry; custom GPU pipelines need their own adapter.

## Compatibility and checks

The connector targets Unreal Engine 5.5+ desktop editor builds, Windows/Linux/macOS,
through public Slate APIs. The local engine used for validation is UE 5.8.4 on
Windows x64. Other engine versions and Linux/macOS builds require validation on
the target installation; this is not a claim of completed testing on all engines.

On that installation, the plugin with All modules and Gallery compiled, a
separate consumer Editor DLL linked against its exported API, and both Unreal
automation tests passed. The rendering checks include a real Unreal texture,
Unicode text entry, context restoration, detaching from inside a draw callback,
and captures for five themes at both densities.

`tests/test_unreal_package.py` checks selective packaging without Unreal. Native
CyUI tests check multiple contexts. The plugin contains Unreal automation tests
`CyUI.Editor.Lifecycle` and `CyUI.Editor.RenderAndInput`. The rendering test needs
a real Slate renderer, including when launched with `-RenderOffscreen`; `-nullrhi`
cannot validate its texture uploads or captures. Run the tests from Session
Frontend or an isolated editor project, for example:

```sh
UnrealEditor-Cmd YourProject.uproject -unattended -RenderOffscreen -nosound -NoLiveCoding -ExecCmds="Automation RunTests CyUI.Editor" -TestExit="Automation Test Queue Empty" -ReportExportPath=Saved/CyUIAutomation
```

Captures are saved in the test project's `Saved/CyUIReview`. The fixture checks
tool registration/unregistration, context restoration, mouse focus, Unicode text
entry and actual Slate screenshots for every built-in palette.

IME composition/candidate placement, accessibility text providers, touch/gamepad
and OS-level ImGui multi-viewports are not implemented by this basic editor
backend. Basic Unicode character events and clipboard text are supported. An
existing third-party Unreal ImGui plugin may use a different ABI/context; do not
share its headers, symbols or texture IDs with this connector.

The generic Web module may be selected, but WebCEF is not packaged into Unreal.
Unreal may already load its own browser runtime; a Web backend for Unreal must
follow that engine's browser lifecycle and version requirements.

## License

CyUI's connector and packaging script remain MIT. Dear ImGui and the optional
TextEditor retain their MIT notices. Unreal Engine remains an external dependency
under [Epic's own license](https://www.unrealengine.com/eula/unreal); it is not redistributed or relicensed by CyUI. Preserve
the plugin's LICENSE and THIRD_PARTY_NOTICES when distributing CyUI code.

API references: [Slate editor window workflow](https://dev.epicgames.com/documentation/en-us/unreal-engine/slate-editor-window-quickstart-guide-for-unreal-engine),
[FSlateVertex](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/SlateCore/FSlateVertex/Make),
[FSlateRenderer](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/SlateCore/FSlateRenderer).
