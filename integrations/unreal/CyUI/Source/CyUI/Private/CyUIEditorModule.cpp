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
    // Reusable service module: the consuming plugin owns tabs, menus and tools.
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
// Expands CYUI_UNREAL_MODULE before IMPLEMENT_MODULE pastes/stringizes the name.
#define CYUI_IMPLEMENT_MODULE(Name) IMPLEMENT_MODULE(FCyUIEditorModule, Name)
CYUI_IMPLEMENT_MODULE(CYUI_UNREAL_MODULE)
