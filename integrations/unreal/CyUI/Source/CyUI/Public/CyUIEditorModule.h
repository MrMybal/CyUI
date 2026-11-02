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
DECLARE_DELEGATE(FOnInitializeCyUITool);

// All calls and draw callbacks run on the editor game thread.
// The callback draws contents; the connector owns ImGui NewFrame/Begin/End/Render.
class CYUI_API FCyUIEditorModule : public IModuleInterface {
  public:
    static FCyUIEditorModule &Get() {
        return FModuleManager::LoadModuleChecked<FCyUIEditorModule>("CyUI");
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
