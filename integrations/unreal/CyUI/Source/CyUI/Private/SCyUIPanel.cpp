#include "SCyUIPanel.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "InputCoreTypes.h"
#include "Misc/App.h"
#include "Misc/ScopeExit.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "RenderingThread.h"
#include "Styling/SlateBrush.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"
#include "cyui/Core.h"
#include <imgui_internal.h>
#include <cfloat>
#include <limits>

namespace {
struct FContextScope {
    ImGuiContext *Previous;
    explicit FContextScope(ImGuiContext *Context) : Previous(ImGui::GetCurrentContext()) {
        ImGui::SetCurrentContext(Context);
    }
    ~FContextScope() {
        ImGui::SetCurrentContext(Previous);
    }
};
int MouseButton(FKey Key) {
    if (Key == EKeys::LeftMouseButton)
        return 0;
    if (Key == EKeys::RightMouseButton)
        return 1;
    if (Key == EKeys::MiddleMouseButton)
        return 2;
    if (Key == EKeys::ThumbMouseButton)
        return 3;
    if (Key == EKeys::ThumbMouseButton2)
        return 4;
    return -1;
}
ImGuiKey KeyCode(FKey Key) {
    const FString Name = Key.GetFName().ToString();
    if (Name.Len() == 1 && Name[0] >= 'A' && Name[0] <= 'Z')
        return ImGuiKey(ImGuiKey_A + Name[0] - 'A');
    static const TPair<FKey, ImGuiKey> Keys[] = {{EKeys::Zero, ImGuiKey_0},
                                                 {EKeys::One, ImGuiKey_1},
                                                 {EKeys::Two, ImGuiKey_2},
                                                 {EKeys::Three, ImGuiKey_3},
                                                 {EKeys::Four, ImGuiKey_4},
                                                 {EKeys::Five, ImGuiKey_5},
                                                 {EKeys::Six, ImGuiKey_6},
                                                 {EKeys::Seven, ImGuiKey_7},
                                                 {EKeys::Eight, ImGuiKey_8},
                                                 {EKeys::Nine, ImGuiKey_9},
                                                 {EKeys::Tab, ImGuiKey_Tab},
                                                 {EKeys::Enter, ImGuiKey_Enter},
                                                 {EKeys::Escape, ImGuiKey_Escape},
                                                 {EKeys::SpaceBar, ImGuiKey_Space},
                                                 {EKeys::BackSpace, ImGuiKey_Backspace},
                                                 {EKeys::Delete, ImGuiKey_Delete},
                                                 {EKeys::Insert, ImGuiKey_Insert},
                                                 {EKeys::Left, ImGuiKey_LeftArrow},
                                                 {EKeys::Right, ImGuiKey_RightArrow},
                                                 {EKeys::Up, ImGuiKey_UpArrow},
                                                 {EKeys::Down, ImGuiKey_DownArrow},
                                                 {EKeys::Home, ImGuiKey_Home},
                                                 {EKeys::End, ImGuiKey_End},
                                                 {EKeys::PageUp, ImGuiKey_PageUp},
                                                 {EKeys::PageDown, ImGuiKey_PageDown},
                                                 {EKeys::LeftShift, ImGuiKey_LeftShift},
                                                 {EKeys::RightShift, ImGuiKey_RightShift},
                                                 {EKeys::LeftControl, ImGuiKey_LeftCtrl},
                                                 {EKeys::RightControl, ImGuiKey_RightCtrl},
                                                 {EKeys::LeftAlt, ImGuiKey_LeftAlt},
                                                 {EKeys::RightAlt, ImGuiKey_RightAlt},
                                                 {EKeys::LeftCommand, ImGuiKey_LeftSuper},
                                                 {EKeys::RightCommand, ImGuiKey_RightSuper},
                                                 {EKeys::F1, ImGuiKey_F1},
                                                 {EKeys::F2, ImGuiKey_F2},
                                                 {EKeys::F3, ImGuiKey_F3},
                                                 {EKeys::F4, ImGuiKey_F4},
                                                 {EKeys::F5, ImGuiKey_F5},
                                                 {EKeys::F6, ImGuiKey_F6},
                                                 {EKeys::F7, ImGuiKey_F7},
                                                 {EKeys::F8, ImGuiKey_F8},
                                                 {EKeys::F9, ImGuiKey_F9},
                                                 {EKeys::F10, ImGuiKey_F10},
                                                 {EKeys::F11, ImGuiKey_F11},
                                                 {EKeys::F12, ImGuiKey_F12},
                                                 {EKeys::NumPadZero, ImGuiKey_Keypad0},
                                                 {EKeys::NumPadOne, ImGuiKey_Keypad1},
                                                 {EKeys::NumPadTwo, ImGuiKey_Keypad2},
                                                 {EKeys::NumPadThree, ImGuiKey_Keypad3},
                                                 {EKeys::NumPadFour, ImGuiKey_Keypad4},
                                                 {EKeys::NumPadFive, ImGuiKey_Keypad5},
                                                 {EKeys::NumPadSix, ImGuiKey_Keypad6},
                                                 {EKeys::NumPadSeven, ImGuiKey_Keypad7},
                                                 {EKeys::NumPadEight, ImGuiKey_Keypad8},
                                                 {EKeys::NumPadNine, ImGuiKey_Keypad9},
                                                 {EKeys::Add, ImGuiKey_KeypadAdd},
                                                 {EKeys::Subtract, ImGuiKey_KeypadSubtract},
                                                 {EKeys::Multiply, ImGuiKey_KeypadMultiply},
                                                 {EKeys::Divide, ImGuiKey_KeypadDivide},
                                                 {EKeys::Decimal, ImGuiKey_KeypadDecimal},
                                                 {EKeys::CapsLock, ImGuiKey_CapsLock},
                                                 {EKeys::NumLock, ImGuiKey_NumLock},
                                                 {EKeys::ScrollLock, ImGuiKey_ScrollLock},
                                                 {EKeys::Pause, ImGuiKey_Pause},
                                                 {EKeys::Semicolon, ImGuiKey_Semicolon},
                                                 {EKeys::Comma, ImGuiKey_Comma},
                                                 {EKeys::Period, ImGuiKey_Period},
                                                 {EKeys::Slash, ImGuiKey_Slash},
                                                 {EKeys::Backslash, ImGuiKey_Backslash},
                                                 {EKeys::LeftBracket, ImGuiKey_LeftBracket},
                                                 {EKeys::RightBracket, ImGuiKey_RightBracket},
                                                 {EKeys::Apostrophe, ImGuiKey_Apostrophe},
                                                 {EKeys::Tilde, ImGuiKey_GraveAccent},
                                                 {EKeys::Hyphen, ImGuiKey_Minus},
                                                 {EKeys::Equals, ImGuiKey_Equal}};
    for (const auto &Pair : Keys)
        if (Pair.Key == Key)
            return Pair.Value;
    return ImGuiKey_None;
}
void Modifiers(const FInputEvent &Event) {
    auto &IO = ImGui::GetIO();
    IO.AddKeyEvent(ImGuiMod_Ctrl, Event.IsControlDown());
    IO.AddKeyEvent(ImGuiMod_Shift, Event.IsShiftDown());
    IO.AddKeyEvent(ImGuiMod_Alt, Event.IsAltDown());
    IO.AddKeyEvent(ImGuiMod_Super, Event.IsCommandDown());
}
// A key goes to exactly one side. ImGui gets it while it wants the keyboard (active item, text
// field, modal, or a focused view that called SetNextFrameWantCaptureKeyboard(true), as webPanel
// does), or for plain navigation keys while ImGui keyboard nav is active. Otherwise only Slate
// sees it, so Ctrl+S, Ctrl+Z, Delete, F7... reach the editor and never run twice.
// io.WantCaptureKeyboard/WantTextInput are refreshed only at the next NewFrame: also read the
// state the last frame left (active item, requests made during it).
bool WantsKeyboard() {
    const auto &G = *ImGui::GetCurrentContext();
    return G.IO.WantCaptureKeyboard || G.IO.WantTextInput || G.ActiveId != 0 ||
           G.WantCaptureKeyboardNextFrame == 1 || G.WantTextInputNextFrame == 1;
}
bool Captures(ImGuiKey Key, const FInputEvent &Event) {
    const auto &IO = ImGui::GetIO();
    if (WantsKeyboard())
        return true;
    const bool Nav = Key == ImGuiKey_Tab || Key == ImGuiKey_LeftArrow || Key == ImGuiKey_RightArrow ||
                     Key == ImGuiKey_UpArrow || Key == ImGuiKey_DownArrow || Key == ImGuiKey_Space ||
                     Key == ImGuiKey_Enter || Key == ImGuiKey_KeypadEnter || Key == ImGuiKey_Escape ||
                     Key == ImGuiKey_PageUp || Key == ImGuiKey_PageDown || Key == ImGuiKey_Home ||
                     Key == ImGuiKey_End;
    return Nav && IO.NavActive && !Event.IsControlDown() && !Event.IsAltDown() && !Event.IsCommandDown();
}
void MousePosition(const FGeometry &Geometry, const FPointerEvent &Event) {
    Modifiers(Event);
    const auto Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
    ImGui::GetIO().AddMousePosEvent(float(Local.X), float(Local.Y));
}
} // namespace
struct SCyUIPanel::FState {
    // An ImGui-managed texture (font atlas...): a transient UTexture2D updated in place.
    struct FAtlas {
        TStrongObjectPtr<UTexture2D> Texture;
        FSlateBrush Brush;
    };
    ImGuiContext *Context = nullptr;
    FOnDrawCyUITool Draw;
    TMap<ImTextureID, TSharedPtr<FAtlas>> Atlases;
    std::string Clipboard;
    ImGuiMouseCursor Cursor = ImGuiMouseCursor_Arrow;
    uint32 Buttons = 0;
    TSet<int32> KeysToImGui; // Pressed keys sent to ImGui: their release goes there too.
    bool bDetached = false, bPainting = false, bDetachRequested = false;
    void ReleaseAtlas(ImTextureID Id) {
        if (!Atlases.Contains(Id))
            return;
        // Finish submitted Slate/render work before dropping the GC pin (rare: atlas rebuilds).
        if (FSlateApplication::IsInitialized())
            FSlateApplication::Get().GetRenderer()->FlushCommands();
        FlushRenderingCommands();
        Atlases.Remove(Id);
    }
    // Converts a rectangle to BGRA8 and queues it on the render thread; the copy is freed there.
    static void Upload(UTexture2D *Object, ImTextureData *Texture, int X, int Y, int Width, int Height) {
        // Without a render resource (-nullrhi) UpdateTextureRegions never runs the cleanup.
        if (!Object->GetResource())
            return;
        const uint32 Pitch = uint32(Width) * 4;
        auto *Bytes = new uint8[size_t(Pitch) * Height];
        const bool bRGBA = Texture->Format == ImTextureFormat_RGBA32;
        for (int Row = 0; Row < Height; ++Row) {
            const auto *Source = static_cast<const uint8 *>(Texture->GetPixelsAt(X, Y + Row));
            uint8 *Target = Bytes + size_t(Row) * Pitch;
            for (int Column = 0; Column < Width; ++Column, Target += 4) {
                Target[0] = bRGBA ? Source[Column * 4 + 2] : 255;
                Target[1] = bRGBA ? Source[Column * 4 + 1] : 255;
                Target[2] = bRGBA ? Source[Column * 4] : 255;
                Target[3] = bRGBA ? Source[Column * 4 + 3] : Source[Column];
            }
        }
        auto *Region = new FUpdateTextureRegion2D(X, Y, 0, 0, Width, Height);
        Object->UpdateTextureRegions(0, 1, Region, Pitch, 4, Bytes,
                                     [](uint8 *Data, const FUpdateTextureRegion2D *Done) {
                                         delete[] Data;
                                         delete Done;
                                     });
    }
    bool UpdateTextures(ImDrawData *Data) {
        if (!Data->Textures)
            return true;
        static uint64 NextAtlas = uint64(1) << 63;
        for (ImTextureData *Texture : *Data->Textures) {
            if (Texture->Status == ImTextureStatus_WantDestroy) {
                ReleaseAtlas(Texture->GetTexID());
                Texture->SetTexID(0);
                Texture->SetStatus(ImTextureStatus_Destroyed);
            } else if (auto *Atlas = Texture->Status == ImTextureStatus_WantUpdates
                                         ? Atlases.Find(Texture->GetTexID())
                                         : nullptr) {
                // In place: only the bounding box of the queued updates is converted and uploaded.
                const auto &R = Texture->UpdateRect;
                if (R.x + R.w > Texture->Width || R.y + R.h > Texture->Height)
                    return false;
                if (R.w > 0 && R.h > 0)
                    Upload((*Atlas)->Texture.Get(), Texture, R.x, R.y, R.w, R.h);
                Texture->SetStatus(ImTextureStatus_OK);
            } else if (Texture->Status == ImTextureStatus_WantCreate ||
                       Texture->Status == ImTextureStatus_WantUpdates) { // Updates without a texture: recreate.
                if (Texture->Width <= 0 || Texture->Height <= 0 ||
                    int64(Texture->Width) * Texture->Height > MAX_int32 / 4)
                    return false;
                UTexture2D *Object = UTexture2D::CreateTransient(Texture->Width, Texture->Height, PF_B8G8R8A8);
                if (!Object)
                    return false;
                Object->Filter = TF_Bilinear;
                // As the previous FSlateDynamicImageBrush path (Slate dynamic image resources are
                // created sRGB, to our reading of the engine source; unverified in-engine).
                Object->SRGB = true;
                Object->UpdateResource();
                auto NewAtlas = MakeShared<FAtlas>();
                NewAtlas->Texture.Reset(Object);
                NewAtlas->Brush.SetResourceObject(Object);
                NewAtlas->Brush.ImageSize = FVector2D(Texture->Width, Texture->Height);
                NewAtlas->Brush.DrawAs = ESlateBrushDrawType::Image;
                Upload(Object, Texture, 0, 0, Texture->Width, Texture->Height);
                const uint64 Id = NextAtlas++;
                ReleaseAtlas(Texture->GetTexID());
                Atlases.Add(Id, NewAtlas);
                Texture->SetTexID(Id);
                Texture->SetStatus(ImTextureStatus_OK);
            }
        }
        return true;
    }
};
SCyUIPanel::SCyUIPanel() = default;
void SCyUIPanel::Construct(const FArguments &Arguments) {
    check(IsInGameThread());
    State = MakeUnique<FState>();
    State->Draw = Arguments._OnDraw;
    ImGuiContext *Previous = ImGui::GetCurrentContext();
    State->Context = ImGui::CreateContext();
    ImGui::SetCurrentContext(State->Context);
    auto &IO = ImGui::GetIO();
    IO.IniFilename = nullptr;
    IO.LogFilename = nullptr;
    IO.BackendPlatformName = "cyui_unreal_slate";
    IO.BackendRendererName = "cyui_unreal_slate";
    IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Keyboard nav alone does not claim the keyboard: editor shortcuts keep working (see Captures).
    IO.ConfigNavCaptureKeyboard = false;
    IO.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures |
                       ImGuiBackendFlags_HasMouseCursors;
    IO.DisplaySize = {640, 480};
    // Slate's render transform scales the panel, so CyUI's logical DPI stays 1 in this context;
    // DisplayFramebufferScale (OnPaint) sets font rasterization density.
    cyui::applyDpi(1);
    cyui::loadFonts();
    auto &Platform = ImGui::GetPlatformIO();
    Platform.Renderer_TextureMaxWidth = Platform.Renderer_TextureMaxHeight = 8192;
    Platform.Platform_ClipboardUserData = State.Get();
    Platform.Platform_GetClipboardTextFn = [](ImGuiContext *Context) -> const char * {
        FContextScope Scope(Context);
        auto *Data = static_cast<FState *>(ImGui::GetPlatformIO().Platform_ClipboardUserData);
        FString Text;
        FPlatformApplicationMisc::ClipboardPaste(Text);
        Data->Clipboard = TCHAR_TO_UTF8(*Text);
        return Data->Clipboard.c_str();
    };
    Platform.Platform_SetClipboardTextFn = [](ImGuiContext *, const char *Text) {
        FPlatformApplicationMisc::ClipboardCopy(UTF8_TO_TCHAR(Text));
    };
    Arguments._OnInitialize.ExecuteIfBound();
    ImGui::SetCurrentContext(Previous);
    SetCanTick(false);
    SetClipping(EWidgetClipping::ClipToBounds);
    ForceVolatile(true);
}
SCyUIPanel::~SCyUIPanel() {
    Detach();
}
void SCyUIPanel::Detach() {
    if (!State || State->bDetached)
        return;
    if (State->bPainting) {
        State->bDetachRequested = true;
        return;
    }
    State->bDetached = true;
    State->Draw.Unbind();
    FContextScope Scope(State->Context);
    TArray<ImTextureID> Ids;
    State->Atlases.GetKeys(Ids);
    for (auto Id : Ids)
        State->ReleaseAtlas(Id);
    for (auto *Texture : ImGui::GetPlatformIO().Textures) {
        Texture->SetTexID(0);
        Texture->SetStatus(ImTextureStatus_Destroyed);
    }
    if (Scope.Previous == State->Context)
        Scope.Previous = nullptr;
    ImGui::DestroyContext(State->Context);
    State->Context = nullptr;
}
void SCyUIPanel::RunInContext(TFunctionRef<void()> Function) {
    check(IsInGameThread());
    if (!State || State->bDetached || !State->Context)
        return;
    FContextScope Scope(State->Context);
    Function();
}
void SCyUIPanel::Notify(const char *Text, cyui::Tone Tone, float Seconds) {
    check(IsInGameThread());
    if (State && !State->bDetached && State->Context)
        cyui::notify(State->Context, Text, Tone, Seconds);
}
FVector2D SCyUIPanel::ComputeDesiredSize(float) const {
    return FVector2D(640, 480);
}
int32 SCyUIPanel::OnPaint(const FPaintArgs &, const FGeometry &Geometry, const FSlateRect &,
                          FSlateWindowElementList &Elements, int32 Layer, const FWidgetStyle &Style,
                          bool bEnabled) const {
    if (!State || State->bDetached || Geometry.GetLocalSize().X <= 0 || Geometry.GetLocalSize().Y <= 0)
        return Layer;
    check(IsInGameThread());
    const auto KeepAlive = SharedThis(this);
    FContextScope Scope(State->Context);
    State->bPainting = true;
    ON_SCOPE_EXIT {
        State->bPainting = false;
        if (State->bDetachRequested) {
            if (Scope.Previous == State->Context)
                Scope.Previous = nullptr;
            const_cast<SCyUIPanel *>(this)->Detach();
        }
    };
    auto &IO = ImGui::GetIO();
    IO.DisplaySize = {float(Geometry.GetLocalSize().X), float(Geometry.GetLocalSize().Y)};
    const float Scale = FMath::Max(.5f, Geometry.GetAccumulatedLayoutTransform().GetScale());
    IO.DisplayFramebufferScale = {Scale, Scale};
    IO.DeltaTime = FMath::Clamp(float(FApp::GetDeltaTime()), .001f, .1f);
    // Theme, density and DPI belong to this panel's context and are applied when they change
    // (Construct, or the tool's own theme calls); nothing is re-applied per paint.
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(IO.DisplaySize);
    if (ImGui::Begin("##CyUIRoot", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings))
        State->Draw.ExecuteIfBound();
    ImGui::End();
    cyui::renderNotifications();
    ImGui::Render();
    State->Cursor = ImGui::GetMouseCursor();
    auto *Data = ImGui::GetDrawData();
    if (!Data || !State->UpdateTextures(Data))
        return Layer;
    const float Opacity = Style.GetColorAndOpacityTint().A * (bEnabled ? 1.f : .5f);
    FSlateRenderer *Renderer = FSlateApplication::Get().GetRenderer();
    FCyUIEditorModule *Module = nullptr; // Looked up once per paint, only for registered textures.
    for (const ImDrawList *List : Data->CmdLists)
        for (const ImDrawCmd &Command : List->CmdBuffer) {
            if (Command.UserCallback) {
                if (Command.UserCallback != ImDrawCallback_ResetRenderState)
                    Command.UserCallback(List, &Command);
                continue;
            }
            if (!Command.ElemCount)
                continue;
            FSlateResourceHandle Resource;
            if (auto *Atlas = State->Atlases.Find(Command.GetTexID()))
                Resource = Renderer->GetResourceHandle((*Atlas)->Brush);
            else {
                if (!Module)
                    Module = &FCyUIEditorModule::Get();
                Resource = Module->ResolveTexture(Command.GetTexID());
            }
            if (!Resource.IsValid())
                continue;
            const auto Clip = Command.ClipRect;
            const FVector2D Low(FMath::Max(0.f, Clip.x - Data->DisplayPos.x),
                                FMath::Max(0.f, Clip.y - Data->DisplayPos.y));
            const FVector2D High(FMath::Min(IO.DisplaySize.x, Clip.z - Data->DisplayPos.x),
                                 FMath::Min(IO.DisplaySize.y, Clip.w - Data->DisplayPos.y));
            if (High.X <= Low.X || High.Y <= Low.Y)
                continue;
            unsigned First = MAX_uint32, Last = 0;
            for (unsigned Index = 0; Index < Command.ElemCount; ++Index) {
                const unsigned Vertex = Command.VtxOffset + List->IdxBuffer[Command.IdxOffset + Index];
                First = FMath::Min(First, Vertex);
                Last = FMath::Max(Last, Vertex);
            }
            if (Last >= unsigned(List->VtxBuffer.Size) ||
                Last - First > std::numeric_limits<SlateIndex>::max())
                continue;
            TArray<FSlateVertex> Vertices;
            Vertices.Reserve(Last - First + 1);
            for (unsigned Index = First; Index <= Last; ++Index) {
                const auto &V = List->VtxBuffer[Index];
                const ImU32 C = V.col;
                const FColor Color((C >> IM_COL32_R_SHIFT) & 255, (C >> IM_COL32_G_SHIFT) & 255,
                                   (C >> IM_COL32_B_SHIFT) & 255,
                                   uint8(((C >> IM_COL32_A_SHIFT) & 255) * Opacity));
                Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
                    Geometry.GetAccumulatedRenderTransform(),
                    FVector2f(V.pos.x - Data->DisplayPos.x, V.pos.y - Data->DisplayPos.y),
                    FVector2f(V.uv.x, V.uv.y), Color));
            }
            TArray<SlateIndex> Indices;
            Indices.Reserve(Command.ElemCount);
            for (unsigned Index = 0; Index < Command.ElemCount; ++Index)
                Indices.Add(
                    SlateIndex(Command.VtxOffset + List->IdxBuffer[Command.IdxOffset + Index] - First));
            const FGeometry ClipGeometry = Geometry.MakeChild(High - Low, FSlateLayoutTransform(Low));
            Elements.PushClip(FSlateClippingZone(ClipGeometry));
            FSlateDrawElement::MakeCustomVerts(Elements, ++Layer, Resource, Vertices, Indices, nullptr, 0, 0);
            Elements.PopClip();
        }
    return Layer;
}
FReply SCyUIPanel::OnMouseButtonDown(const FGeometry &Geometry, const FPointerEvent &Event) {
    if (!State || State->bDetached || MouseButton(Event.GetEffectingButton()) < 0)
        return FReply::Unhandled();
    FContextScope Scope(State->Context);
    MousePosition(Geometry, Event);
    const int Button = MouseButton(Event.GetEffectingButton());
    State->Buttons |= 1u << Button;
    ImGui::GetIO().AddMouseButtonEvent(Button, true);
    return FReply::Handled()
        .SetUserFocus(SharedThis(this), EFocusCause::Mouse)
        .CaptureMouse(SharedThis(this));
}
FReply SCyUIPanel::OnMouseButtonUp(const FGeometry &Geometry, const FPointerEvent &Event) {
    if (!State || State->bDetached || MouseButton(Event.GetEffectingButton()) < 0)
        return FReply::Unhandled();
    FContextScope Scope(State->Context);
    MousePosition(Geometry, Event);
    const int Button = MouseButton(Event.GetEffectingButton());
    State->Buttons &= ~(1u << Button);
    ImGui::GetIO().AddMouseButtonEvent(Button, false);
    return State->Buttons ? FReply::Handled() : FReply::Handled().ReleaseMouseCapture();
}
FReply SCyUIPanel::OnMouseButtonDoubleClick(const FGeometry &Geometry, const FPointerEvent &Event) {
    return OnMouseButtonDown(Geometry, Event);
}
FReply SCyUIPanel::OnMouseMove(const FGeometry &Geometry, const FPointerEvent &Event) {
    if (!State || State->bDetached)
        return FReply::Unhandled();
    FContextScope Scope(State->Context);
    MousePosition(Geometry, Event);
    return FReply::Handled();
}
FReply SCyUIPanel::OnMouseWheel(const FGeometry &Geometry, const FPointerEvent &Event) {
    if (!State || State->bDetached)
        return FReply::Unhandled();
    FContextScope Scope(State->Context);
    MousePosition(Geometry, Event);
    // Vertical only: ImGui itself turns Shift+wheel into horizontal scrolling.
    ImGui::GetIO().AddMouseWheelEvent(0, Event.GetWheelDelta());
    return FReply::Handled();
}
void SCyUIPanel::OnMouseLeave(const FPointerEvent &Event) {
    SLeafWidget::OnMouseLeave(Event);
    if (State && !State->bDetached && !HasMouseCapture()) {
        FContextScope Scope(State->Context);
        ImGui::GetIO().AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    }
}
void SCyUIPanel::OnMouseCaptureLost(const FCaptureLostEvent &Event) {
    SLeafWidget::OnMouseCaptureLost(Event);
    if (State && !State->bDetached) {
        FContextScope Scope(State->Context);
        for (int Button = 0; Button < 5; ++Button)
            ImGui::GetIO().AddMouseButtonEvent(Button, false);
        State->Buttons = 0;
    }
}
FReply SCyUIPanel::OnKeyDown(const FGeometry &, const FKeyEvent &Event) {
    if (!State || State->bDetached)
        return FReply::Unhandled();
    FContextScope Scope(State->Context);
    Modifiers(Event);
    const auto Key = KeyCode(Event.GetKey());
    if (Key == ImGuiKey_None)
        return FReply::Unhandled();
    if (State->KeysToImGui.Contains(Key))
        return FReply::Handled(); // Repeat of a key ImGui owns; ImGui repeats by itself.
    if (Event.IsRepeat() || !Captures(Key, Event))
        return FReply::Unhandled();
    State->KeysToImGui.Add(Key);
    ImGui::GetIO().AddKeyEvent(Key, true);
    return FReply::Handled();
}
FReply SCyUIPanel::OnKeyUp(const FGeometry &, const FKeyEvent &Event) {
    if (!State || State->bDetached)
        return FReply::Unhandled();
    FContextScope Scope(State->Context);
    Modifiers(Event);
    const auto Key = KeyCode(Event.GetKey());
    if (Key == ImGuiKey_None || !State->KeysToImGui.Remove(Key))
        return FReply::Unhandled();
    ImGui::GetIO().AddKeyEvent(Key, false);
    return FReply::Handled();
}
FReply SCyUIPanel::OnKeyChar(const FGeometry &, const FCharacterEvent &Event) {
    if (!State || State->bDetached)
        return FReply::Unhandled();
    FContextScope Scope(State->Context);
    Modifiers(Event);
    // Characters, like keys, go to one side: ImGui only while it takes keyboard input.
    if (!WantsKeyboard())
        return FReply::Unhandled();
    // Slate supplies UTF-16 units on Windows, Unicode scalars on UTF-32 platforms.
    if constexpr (sizeof(TCHAR) == 2)
        ImGui::GetIO().AddInputCharacterUTF16(ImWchar16(Event.GetCharacter()));
    else
        ImGui::GetIO().AddInputCharacter(unsigned(Event.GetCharacter()));
    return FReply::Handled();
}
FReply SCyUIPanel::OnFocusReceived(const FGeometry &, const FFocusEvent &) {
    if (State && !State->bDetached) {
        FContextScope Scope(State->Context);
        ImGui::GetIO().AddFocusEvent(true);
    }
    return FReply::Handled();
}
void SCyUIPanel::OnFocusLost(const FFocusEvent &Event) {
    SLeafWidget::OnFocusLost(Event);
    if (State && !State->bDetached) {
        FContextScope Scope(State->Context);
        ImGui::GetIO().AddFocusEvent(false); // Releases every key ImGui holds.
        State->KeysToImGui.Empty();
    }
}
FCursorReply SCyUIPanel::OnCursorQuery(const FGeometry &, const FPointerEvent &) const {
    const auto Cursor = State ? State->Cursor : ImGuiMouseCursor_Arrow;
    EMouseCursor::Type Type = EMouseCursor::Default;
    switch (Cursor) {
    case ImGuiMouseCursor_TextInput:
        Type = EMouseCursor::TextEditBeam;
        break;
    case ImGuiMouseCursor_ResizeNS:
        Type = EMouseCursor::ResizeUpDown;
        break;
    case ImGuiMouseCursor_ResizeEW:
        Type = EMouseCursor::ResizeLeftRight;
        break;
    case ImGuiMouseCursor_ResizeNESW:
        Type = EMouseCursor::ResizeSouthWest;
        break;
    case ImGuiMouseCursor_ResizeNWSE:
        Type = EMouseCursor::ResizeSouthEast;
        break;
    case ImGuiMouseCursor_ResizeAll:
        Type = EMouseCursor::CardinalCross;
        break;
    case ImGuiMouseCursor_Hand:
        Type = EMouseCursor::Hand;
        break;
    case ImGuiMouseCursor_NotAllowed:
        Type = EMouseCursor::SlashedCircle;
        break;
    case ImGuiMouseCursor_None:
        Type = EMouseCursor::None;
        break;
    default:
        break;
    }
    return FCursorReply::Cursor(Type);
}
