#include "CyUIEditorModule.h"
#include "SCyUIPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Styling/SlateBrush.h"
#include "Engine/Texture.h"
#include "UObject/StrongObjectPtr.h"
#include "Rendering/SlateRenderer.h"
#include "RenderingThread.h"
#include "ToolMenus.h"
#include "cyui/CyUI.h"

namespace {
void DrawCyUIExample();
}
struct FCyUIEditorModule::FTool {
    FText Label;
    FOnDrawCyUITool Draw;
    FOnInitializeCyUITool Initialize;
    TWeakPtr<SDockTab> Tab;
    TArray<TWeakPtr<SCyUIPanel>> Panels;
};
struct FCyUIEditorModule::FTexture {
    TStrongObjectPtr<UTexture> Object;
    FSlateBrush Brush;
    explicit FTexture(UTexture *Texture) : Object(Texture) {
        Brush.SetResourceObject(Texture);
        Brush.DrawAs = ESlateBrushDrawType::Image;
        Brush.ImageType = ESlateBrushImageType::FullColor;
    }
};
void FCyUIEditorModule::StartupModule() {
    if (IsRunningCommandlet())
        return;
    RegisterTool("CyUI.Example", NSLOCTEXT("CyUI", "Example", "CyUI Tools"),
                 FOnDrawCyUITool::CreateStatic(&DrawCyUIExample));
#if defined(CYUI_HAS_GALLERY)
    RegisterTool("CyUI.Gallery", NSLOCTEXT("CyUI", "Gallery", "CyUI Gallery"),
                 FOnDrawCyUITool::CreateLambda([] { cyui::showGallery(); }));
#endif
    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FCyUIEditorModule::RegisterMenus));
}
void FCyUIEditorModule::RegisterMenus() {
    FToolMenuOwnerScoped Owner(this);
    UToolMenu *Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
    Menu->AddSection("CyUI", NSLOCTEXT("CyUI", "Tools", "CyUI"))
        .AddDynamicEntry("CyUITools",
                         FNewToolMenuSectionDelegate::CreateLambda([this](FToolMenuSection &Section) {
                             TArray<FName> Names;
                             Tools.GetKeys(Names);
                             Names.Sort(FNameLexicalLess());
                             for (FName Id : Names)
                                 Section.AddMenuEntry(
                                     Id, Tools[Id]->Label, FText::GetEmpty(), FSlateIcon(),
                                     FUIAction(FExecuteAction::CreateLambda([this, Id] { OpenTool(Id); })));
                         }));
}
bool FCyUIEditorModule::RegisterTool(FName Id, FText Label, FOnDrawCyUITool Draw,
                                     FOnInitializeCyUITool Initialize) {
    check(IsInGameThread());
    if (!FSlateApplication::IsInitialized() || IsRunningCommandlet() || Id.IsNone() || !Draw.IsBound() ||
        Tools.Contains(Id))
        return false;
    auto Tool = MakeShared<FTool>();
    Tool->Label = Label;
    Tool->Draw = MoveTemp(Draw);
    Tool->Initialize = MoveTemp(Initialize);
    Tools.Add(Id, Tool);
    FGlobalTabmanager::Get()
        ->RegisterNomadTabSpawner(Id, FOnSpawnTab::CreateRaw(this, &FCyUIEditorModule::SpawnTool, Id))
        .SetDisplayName(Label)
        .SetMenuType(ETabSpawnerMenuType::Hidden);
    return true;
}
TSharedRef<SDockTab> FCyUIEditorModule::SpawnTool(const FSpawnTabArgs &, FName Id) {
    auto Tool = Tools.FindChecked(Id);
    TSharedRef<SCyUIPanel> Panel = SNew(SCyUIPanel).OnDraw(Tool->Draw).OnInitialize(Tool->Initialize);
    Panels.RemoveAll([](const auto &Weak) { return !Weak.IsValid(); });
    Tool->Panels.RemoveAll([](const auto &Weak) { return !Weak.IsValid(); });
    Panels.Add(Panel);
    Tool->Panels.Add(Panel);
    TSharedRef<SDockTab> Tab = SNew(SDockTab).TabRole(ETabRole::NomadTab)[Panel];
    Tool->Tab = Tab;
    return Tab;
}
bool FCyUIEditorModule::OpenTool(FName Id) {
    check(IsInGameThread());
    return Tools.Contains(Id) && FGlobalTabmanager::Get()->TryInvokeTab(Id).IsValid();
}
TSharedRef<SWidget> FCyUIEditorModule::MakePanel(FOnDrawCyUITool Draw, FOnInitializeCyUITool Initialize) {
    check(IsInGameThread());
    TSharedRef<SCyUIPanel> Panel = SNew(SCyUIPanel).OnDraw(MoveTemp(Draw)).OnInitialize(MoveTemp(Initialize));
    Panels.RemoveAll([](const auto &Weak) { return !Weak.IsValid(); });
    Panels.Add(Panel);
    return Panel;
}
void FCyUIEditorModule::UnregisterTool(FName Id) {
    check(IsInGameThread());
    if (auto *Found = Tools.Find(Id)) {
        for (auto &Weak : (*Found)->Panels)
            if (auto Panel = Weak.Pin())
                Panel->Detach();
        if (auto Tab = (*Found)->Tab.Pin())
            Tab->RequestCloseTab();
        (*Found)->Draw.Unbind();
        (*Found)->Initialize.Unbind();
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(Id);
        Tools.Remove(Id);
    }
}
uint64 FCyUIEditorModule::RegisterTexture(UTexture *Texture) {
    check(IsInGameThread());
    if (!IsValid(Texture))
        return 0;
    const uint64 Id = NextTexture++;
    Textures.Add(Id, MakeShared<FTexture>(Texture));
    return Id;
}
void FCyUIEditorModule::UnregisterTexture(uint64 Id) {
    check(IsInGameThread());
    if (!Textures.Contains(Id))
        return;
    // Finish already submitted Slate work before releasing the GC pin.
    if (FSlateApplication::IsInitialized())
        FSlateApplication::Get().GetRenderer()->FlushCommands();
    FlushRenderingCommands();
    Textures.Remove(Id);
}
FSlateResourceHandle FCyUIEditorModule::ResolveTexture(uint64 Id) const {
    if (auto *Found = Textures.Find(Id))
        return FSlateApplication::Get().GetRenderer()->GetResourceHandle((*Found)->Brush);
    return {};
}
void FCyUIEditorModule::ShutdownModule() {
    if (IsRunningCommandlet())
        return;
    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);
    for (auto &Weak : Panels)
        if (auto Panel = Weak.Pin())
            Panel->Detach();
    TArray<FName> Ids;
    Tools.GetKeys(Ids);
    for (FName Id : Ids)
        UnregisterTool(Id);
    TArray<uint64> TextureIds;
    Textures.GetKeys(TextureIds);
    for (uint64 Id : TextureIds)
        UnregisterTexture(Id);
    Panels.Empty();
}
namespace {
void DrawCyUIExample() {
    cyui::heading("CyUI editor tools", 1);
    cyui::textDim("This interface is drawn inside a dockable Unreal Editor tab.");
    if (ImGui::BeginCombo("Theme", cyui::currentThemeKey().c_str())) {
        for (const auto &Theme : cyui::builtInThemes())
            if (ImGui::Selectable(Theme.name.c_str(), Theme.key == cyui::currentThemeKey()))
                cyui::setTheme(Theme.key.c_str());
        ImGui::EndCombo();
    }
    static float Strength = .5f;
    static bool Enabled = true;
    static char Name[256] = "Editor tool";
    if (cyui::beginProperties("Example properties")) {
        cyui::property("Name");
        cyui::textField("##name", Name, sizeof(Name));
        cyui::property("Enabled");
        cyui::toggleSwitch("##enabled", &Enabled);
        cyui::property("Strength");
        cyui::sliderBar("##strength", &Strength, 0, 1);
        cyui::endProperties();
    }
    if (cyui::primaryButton("Run example"))
        cyui::notify("Tool action received", cyui::Tone::success);
#if defined(CYUI_HAS_PLOTS)
    static cyui::PlotState Plot;
    static const std::vector<cyui::Point> Points{{0, 0}, {.2, .8}, {.4, .3}, {.6, 1}, {.8, .5}, {1, .7}};
    cyui::PlotSeries Series{1, "Preview", Points};
    cyui::plot("Preview", Plot, {&Series, 1}, cyui::dp(0, 180));
#endif
}
} // namespace
IMPLEMENT_MODULE(FCyUIEditorModule, CyUI)
