#pragma once
#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Rendering/SlateResourceHandle.h"
#include "cyui/Export.h"

class SWidget;
class SCyUIPanel;
class SDockTab;
class UTexture;
class FSpawnTabArgs;
struct FSlateBrush;
DECLARE_DELEGATE(FOnDrawCyUITool);
// The module's own name, set by its Build.cs (renamed copies: tools/package_unreal.py --module-name).
#ifndef CYUI_UNREAL_MODULE
#define CYUI_UNREAL_MODULE CyUI
#endif
#define CYUI_UNREAL_STRINGIFY_(Name) #Name
#define CYUI_UNREAL_STRINGIFY(Name) CYUI_UNREAL_STRINGIFY_(Name)
DECLARE_DELEGATE(FOnInitializeCyUITool);

// All calls and draw callbacks run on the editor game thread.
// The callback draws contents; the connector owns ImGui NewFrame/Begin/End/Render.
class CYUI_API FCyUIEditorModule : public IModuleInterface {
  public:
    static FName ModuleName() {
        return FName(CYUI_UNREAL_STRINGIFY(CYUI_UNREAL_MODULE));
    }
    static FCyUIEditorModule &Get() {
        return FModuleManager::LoadModuleChecked<FCyUIEditorModule>(ModuleName());
    }
    static bool IsAvailable() {
        return FModuleManager::Get().IsModuleLoaded(ModuleName());
    }
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    virtual bool SupportsDynamicReloading() override {
        return false;
    }
    bool RegisterTool(FName Id, FText Label, FOnDrawCyUITool Draw, FOnInitializeCyUITool Initialize = {});
    void UnregisterTool(FName Id);
    bool OpenTool(FName Id);
    TSharedRef<SWidget> MakePanel(FOnDrawCyUITool Draw, FOnInitializeCyUITool Initialize = {});
    // Pins a texture until UnregisterTexture. IDs are specific to this connector.
    uint64 RegisterTexture(UTexture *Texture);
    void UnregisterTexture(uint64 Id);
    FSlateResourceHandle ResolveTexture(uint64 Id) const;

  private:
    struct FTool;
    struct FTexture;
    TMap<FName, TSharedPtr<FTool>> Tools;
    TMap<uint64, TSharedPtr<FTexture>> Textures;
    TArray<TWeakPtr<SCyUIPanel>> Panels;
    uint64 NextTexture = 1;
    TSharedRef<SDockTab> SpawnTool(const FSpawnTabArgs &, FName Id);
};
