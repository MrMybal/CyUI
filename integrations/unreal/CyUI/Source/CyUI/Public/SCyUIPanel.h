#pragma once
#include "Widgets/SLeafWidget.h"
#include "CyUIEditorModule.h"

// A single ImGui context per Slate panel. Unreal owns OS windows and docking.
class CYUI_API SCyUIPanel : public SLeafWidget {
  public:
    SCyUIPanel();
    SLATE_BEGIN_ARGS(SCyUIPanel) {}
    SLATE_EVENT(FOnDrawCyUITool, OnDraw)
    SLATE_EVENT(FOnInitializeCyUITool, OnInitialize)
    SLATE_END_ARGS()
    void Construct(const FArguments &);
    virtual ~SCyUIPanel() override;
    void Detach();
    virtual bool SupportsKeyboardFocus() const override {
        return true;
    }
    virtual FVector2D ComputeDesiredSize(float) const override;
    virtual int32 OnPaint(const FPaintArgs &, const FGeometry &, const FSlateRect &,
                          FSlateWindowElementList &, int32, const FWidgetStyle &, bool) const override;
    virtual FReply OnMouseButtonDown(const FGeometry &, const FPointerEvent &) override;
    virtual FReply OnMouseButtonUp(const FGeometry &, const FPointerEvent &) override;
    virtual FReply OnMouseButtonDoubleClick(const FGeometry &, const FPointerEvent &) override;
    virtual FReply OnMouseMove(const FGeometry &, const FPointerEvent &) override;
    virtual FReply OnMouseWheel(const FGeometry &, const FPointerEvent &) override;
    virtual void OnMouseLeave(const FPointerEvent &) override;
    virtual void OnMouseCaptureLost(const FCaptureLostEvent &) override;
    virtual FReply OnKeyDown(const FGeometry &, const FKeyEvent &) override;
    virtual FReply OnKeyUp(const FGeometry &, const FKeyEvent &) override;
    virtual FReply OnKeyChar(const FGeometry &, const FCharacterEvent &) override;
    virtual FReply OnFocusReceived(const FGeometry &, const FFocusEvent &) override;
    virtual void OnFocusLost(const FFocusEvent &) override;
    virtual FCursorReply OnCursorQuery(const FGeometry &, const FPointerEvent &) const override;

  private:
    struct FState;
    TUniquePtr<FState> State;
};
