---
id: recipes
title: Recettes du Core
description: Assembler un inspecteur, une palette d’outils et une confirmation.
section: Guides
parent: design
order: 12
sourceRevision: 1
---

Ces fragments s’insèrent dans la frame ImGui de l’application. Les modèles et actions cités sont fournis par l’hôte. Pour une interface traduite, appliquez `trLabel` aux libellés interactifs et `tr` aux textes ; voir [la localisation](doc:localization).

**Panneau d’inspection** : regroupez les champs dans une table de propriétés et fermez les sections uniquement lorsqu’elles sont ouvertes.

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

**Palette d’outils** : conservez la sélection dans l’application et fournissez des infobulles explicites.

```cpp
for (int i = 0; i < toolCount; ++i) {
    if (cyui::iconButton(tools[i].icon, tools[i].tooltip, current == i))
        current = i;
    ImGui::SameLine(0, cyui::dp(2));
}
cyui::toolbarSeparator();
```

**Confirmation** : le résultat 1 confirme, le résultat 2 annule ; l’action métier reste dans l’hôte.

```cpp
if (cyui::beginModal("Discard changes?", &askDiscard)) {
    ImGui::TextWrapped("The project has unsaved changes.");
    if (cyui::modalButtons("Discard", "Cancel", true) == 1)
        discard();
    cyui::endModal();
}
```

**Couleurs d’un canevas personnalisé** : toutes les couleurs de l’interface viennent de rôles et toutes les tailles d’écran du style ou de `dp`.

```cpp
const auto& p = cyui::palette();
draw->AddRectFilled(min, max, p.bg0);
for (...) draw->AddLine(a, b, major ? p.gridStrong : p.grid);
draw->AddRectFilled(selMin, selMax, p.selection);
draw->AddRect(selMin, selMax, p.accent, 0, 0, cyui::dp(1.5f));
```

```image
{"id": "recipes-view", "src": "assets/inspector.webp", "alt": "Détails groupés d’un éditeur", "controls": "minimal"}
```
