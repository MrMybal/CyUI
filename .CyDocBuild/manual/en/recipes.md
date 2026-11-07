---
id: recipes
title: Core recipes
description: Core recipes for CyUI integrators.
section: Guides
parent: design
order: 12
revision: 1
---

**Inspector panel**
```cpp
if (ImGui::Begin("Inspector")) {
    cyui::panelHeader("Layer", layer.name.c_str());
    cyui::divider();
    if (cyui::beginSection("Transform")) {
        if (cyui::beginProperties("##transform")) {
            cyui::property("Position");  ImGui::DragFloat2("##pos", &layer.x);
            cyui::property("Opacity");   cyui::sliderBar("##opacity", &layer.opacity, 0, 1, "%.2f", -1);
            cyui::property("Visible");   cyui::toggleSwitch("##visible", &layer.visible);
            cyui::property("Blend", "How the layer mixes with the ones under it.");
            cyui::combo("##blend", &layer.blend, blendNames, blendCount);
            cyui::endProperties();
        }
        cyui::endSection();
    }
}
ImGui::End();
```

**Tool palette**
```cpp
for (int i = 0; i < toolCount; ++i) {
    if (cyui::iconButton(tools[i].icon, tools[i].tooltip, current == i))
        current = i;
    ImGui::SameLine(0, cyui::dp(2));
}
cyui::toolbarSeparator();
```

**Confirm dialog**
```cpp
if (cyui::beginModal("Discard changes?", &askDiscard)) {
    ImGui::TextWrapped("The project has unsaved changes.");
    if (cyui::modalButtons("Discard", "Cancel", true) == 1)
        discard();
    cyui::endModal();
}
```

**Custom canvas colours**
```cpp
const auto& p = cyui::palette();
draw->AddRectFilled(min, max, p.bg0);
for (...) draw->AddLine(a, b, major ? p.gridStrong : p.grid);
draw->AddRectFilled(selMin, selMax, p.selection);
draw->AddRect(selMin, selMax, p.accent, 0, 0, cyui::dp(1.5f));
```

---

```image
{"id": "recipes-view", "src": "assets/inspector.webp", "alt": "Grouped editor details", "controls": "minimal"}
```
