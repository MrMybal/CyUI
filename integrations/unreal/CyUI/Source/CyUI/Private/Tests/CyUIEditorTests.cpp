#include "CyUIEditorModule.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "SCyUIPanel.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "InputCoreTypes.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "cyui/Core.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCyUILifecycleTest, "CyUI.Editor.Lifecycle",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCyUILifecycleTest::RunTest(const FString &) {
    auto &Module = FCyUIEditorModule::Get();
    TestFalse(TEXT("Loading the service does not register a demo tool"), Module.OpenTool("CyUI.Example"));
    TestFalse(TEXT("Loading the service does not register a gallery tool"), Module.OpenTool("CyUI.Gallery"));
    const FName Id("CyUI.AutomationTool");
    const FText Label = FText::FromString("CyUI test tool");
    auto Draw = FOnDrawCyUITool::CreateLambda([] { cyui::heading("Automation tool"); });
    TestTrue(TEXT("Register tool"), Module.RegisterTool(Id, Label, Draw));
    TestFalse(TEXT("Reject duplicate registration"), Module.RegisterTool(Id, Label, Draw));
    ImGuiContext *Before = ImGui::GetCurrentContext();
    auto First = StaticCastSharedRef<SCyUIPanel>(Module.MakePanel(Draw));
    auto Second = StaticCastSharedRef<SCyUIPanel>(Module.MakePanel(Draw));
    First->RunInContext([this] {
        TestTrue(TEXT("Panel starts in English"), cyui::locale() == "en");
        TestTrue(TEXT("Panel can switch to French"), cyui::setLocale("fr"));
        TestTrue(TEXT("French catalog is available inside Unreal"), cyui::tr("Save") == "Enregistrer");
    });
    Second->RunInContext([this] {
        TestTrue(TEXT("Another panel keeps English"), cyui::locale() == "en");
    });
    First->RunInContext([this] {
        TestTrue(TEXT("Panel keeps its selected language"), cyui::locale() == "fr");
    });
    TestTrue(TEXT("Panel creation restores the caller context"), Before == ImGui::GetCurrentContext());
    TestTrue(TEXT("Open registered dockable tab"), Module.OpenTool(Id));
    Module.UnregisterTool(Id);
    TestFalse(TEXT("Unregistered tab cannot open"), Module.OpenTool(Id));
    TestEqual(TEXT("Null texture rejected"), Module.RegisterTexture(nullptr), uint64(0));
    TestTrue(TEXT("Unregistration restores the caller context"), Before == ImGui::GetCurrentContext());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCyUIRenderTest, "CyUI.Editor.RenderAndInput",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCyUIRenderTest::RunTest(const FString &) {
    struct FInputState {
        char Text[256]{};
        ImVec2 Position{};
        uint64 Texture = 0;
        int Draws = 0;
        bool Close = false;
        TWeakPtr<SCyUIPanel> Panel;
    };
    auto Input = MakeShared<FInputState>();
    auto *Texture = UTexture2D::CreateTransient(8, 8, PF_B8G8R8A8);
    if (!Texture) {
        AddError(TEXT("Cannot create fixture texture"));
        return false;
    }
    auto *TexturePixels =
        static_cast<FColor *>(Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
    const auto C = cyui::palette().info;
    for (int Pixel = 0; Pixel < 64; ++Pixel)
        TexturePixels[Pixel] = FColor((C >> IM_COL32_R_SHIFT) & 255, (C >> IM_COL32_G_SHIFT) & 255,
                                      (C >> IM_COL32_B_SHIFT) & 255, 255);
    Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
    Texture->UpdateResource();
    Input->Texture = FCyUIEditorModule::Get().RegisterTexture(Texture);
    auto Panel = SNew(SCyUIPanel)
                     .OnDraw(FOnDrawCyUITool::CreateLambda([Input] {
                         ++Input->Draws;
                         if (Input->Close) {
                             if (auto Self = Input->Panel.Pin())
                                 Self->Detach();
                             return;
                         }
                         cyui::heading("CyUI / Unreal Slate", 1);
                         cyui::textDim("Dockable editor panel, dynamic font atlas and Unicode input.");
                         cyui::textField("Unicode", Input->Text, sizeof(Input->Text));
                         const auto Low = ImGui::GetItemRectMin(), High = ImGui::GetItemRectMax();
                         Input->Position = {(Low.x + High.x) * .5f, (Low.y + High.y) * .5f};
                         if (cyui::beginCard("Example", "Editor tool")) {
                             cyui::badge("SLATE", cyui::Tone::info);
                             cyui::callout(cyui::Tone::success, "CyUI draws through the editor renderer.");
                             cyui::primaryButton("Tool action");
                         }
                         cyui::endCard();
                         ImGui::Image(ImTextureRef(Input->Texture), cyui::dp(64, 64));
                     }))
                     .OnInitialize(FOnInitializeCyUITool::CreateLambda([] {
                         const FString Font =
                             FPaths::EngineContentDir() / TEXT("Slate/Fonts/DroidSansFallback.ttf");
                         ImFontConfig Config;
                         Config.MergeMode = true;
                         Config.DstFont = cyui::uiFont();
                         ImGui::GetIO().Fonts->AddFontFromFileTTF(TCHAR_TO_UTF8(*Font),
                                                                  cyui::metrics().fontSize, &Config);
                     }));
    Input->Panel = Panel;
    auto Window = SNew(SWindow)
                      .Title(FText::FromString("CyUI integration test"))
                      .ClientSize(FVector2D(900, 650))[Panel];
    FSlateApplication::Get().AddWindow(Window);
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FDelayedFunctionLatentCommand>(
        [this, Panel, Window, Input] {
            const auto Before = ImGui::GetCurrentContext();
            TArray<FColor> Pixels;
            FIntVector Size;
            // Warm up layout and the 1.92 renderer-managed font atlas.
            for (int Frame = 0; Frame < 3; ++Frame)
                FSlateApplication::Get().TakeScreenshot(Panel, Pixels, Size);
            const FGeometry Geometry = Panel->GetCachedGeometry();
            const auto At = Geometry.LocalToAbsolute(FVector2D(Input->Position.x, Input->Position.y));
            FPointerEvent Down(0, At, At, TSet<FKey>{EKeys::LeftMouseButton}, EKeys::LeftMouseButton, 0,
                               FModifierKeysState());
            Panel->OnFocusReceived(Geometry, FFocusEvent(EFocusCause::SetDirectly, 0));
            Panel->OnMouseMove(Geometry, Down);
            FSlateApplication::Get().TakeScreenshot(Panel, Pixels, Size);
            Panel->OnMouseButtonDown(Geometry, Down);
            FSlateApplication::Get().TakeScreenshot(Panel, Pixels, Size);
            FPointerEvent Up(0, At, At, TSet<FKey>{}, EKeys::LeftMouseButton, 0, FModifierKeysState());
            Panel->OnMouseButtonUp(Geometry, Up);
            Panel->OnKeyChar(Geometry, FCharacterEvent(TCHAR(0x97f3), FModifierKeysState(), 0, false));
            Panel->OnKeyChar(Geometry, FCharacterEvent(TCHAR(0x697d), FModifierKeysState(), 0, false));
            for (int Frame = 0; Frame < 3; ++Frame)
                FSlateApplication::Get().TakeScreenshot(Panel, Pixels, Size);
            TestEqual(TEXT("Mouse focus and Unicode input reach the ImGui text field"),
                      FString(UTF8_TO_TCHAR(Input->Text)), FString(TEXT("\u97f3\u697d")));
            TestTrue(TEXT("Unreal texture resolves through the registry"),
                     FCyUIEditorModule::Get().ResolveTexture(Input->Texture).IsValid());
            // A toast queued from outside the draw callback lands in this panel's context.
            Panel->Notify("Automation notification", cyui::Tone::info);
            const auto Folder = FPaths::ProjectSavedDir() / TEXT("CyUIReview");
            IFileManager::Get().MakeDirectory(*Folder, true);
            // Theme state is per ImGui context: change the panel's own, and restore it afterwards.
            std::string SavedTheme;
            cyui::Density SavedDensity = cyui::Density::compact;
            Panel->RunInContext([&] {
                SavedTheme = cyui::currentThemeKey();
                SavedDensity = cyui::density();
            });
            for (const auto &Theme : cyui::builtInThemes())
                for (int Density = 0; Density < 2; ++Density) {
                    Panel->RunInContext([&] {
                        cyui::setTheme(Theme.key.c_str());
                        cyui::setDensity(Density ? cyui::Density::comfortable : cyui::Density::compact);
                    });
                    // Let cached auto-height layouts settle after changing density.
                    for (int Frame = 0; Frame < 2; ++Frame)
                        FSlateApplication::Get().TakeScreenshot(Panel, Pixels, Size);
                    TestTrue(TEXT("Real Slate screenshot"),
                             FSlateApplication::Get().TakeScreenshot(Panel, Pixels, Size));
                    if (Pixels.Num() && Size.X > 0 && Size.Y > 0)
                        TestTrue(TEXT("Save theme capture"),
                                 FFileHelper::CreateBitmap(
                                     *(Folder / (FString(UTF8_TO_TCHAR(Theme.key.c_str())) +
                                                 (Density ? TEXT("-comfortable") : TEXT("-compact")))),
                                     Size.X, Size.Y, Pixels.GetData()));
                    TSet<uint32> Colors;
                    for (const auto &Color : Pixels)
                        Colors.Add(Color.DWColor());
                    TestTrue(TEXT("Screenshot contains rendered controls and text"), Colors.Num() > 16);
                }
            Panel->RunInContext([&] {
                cyui::setTheme(SavedTheme.c_str());
                cyui::setDensity(SavedDensity);
            });
            TestTrue(TEXT("Paint restores the caller context"), Before == ImGui::GetCurrentContext());
            Input->Close = true;
            FSlateApplication::Get().TakeScreenshot(Panel, Pixels, Size);
            const int AfterClose = Input->Draws;
            FSlateApplication::Get().TakeScreenshot(Panel, Pixels, Size);
            TestEqual(TEXT("A panel can detach inside its callback without further drawing"), Input->Draws,
                      AfterClose);
            FCyUIEditorModule::Get().UnregisterTexture(Input->Texture);
            TestFalse(TEXT("Released texture IDs are rejected"),
                      FCyUIEditorModule::Get().ResolveTexture(Input->Texture).IsValid());
            Window->RequestDestroyWindow();
        },
        .5f));
    return true;
}
#endif
