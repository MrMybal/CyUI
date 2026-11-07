---
id: filtering
title: Filtres rapides pour menus et panneaux
description: Retrouver des actions et propriétés sans modifier les modèles.
section: Guides
order: 32
sourceRevision: 1
---

Le filtrage est inclus dans `CyUI::Core` et `<cyui/Core.h>`, activé par vue. L’hôte décide quels contrôles, propriétés et actions correspondent au filtre. Gardez un `TextFilter` par instance de panneau/menu pour éviter les requêtes partagées. Les appels ImGui bruts ne sont pas automatiquement masqués et Dear ImGui n’est pas modifié.

## Filtrer un panneau ou une liste {#panel-and-list-filtering}

```cpp
// Persist alongside the panel's other state.
cyui::TextFilter filter;

// During the frame, inside the panel's ImGui window:
cyui::panelHeader(cyui::tr("Properties").c_str(), nullptr, &filter);
if (cyui::beginProperties("##properties")) {
    if (filter.matches("Opacity", "Surface")) {
        cyui::property(cyui::tr("Opacity").c_str());
        cyui::sliderBar("##opacity", &opacity, 0, 1, "%.2f", -1);
    }
    cyui::endProperties();
}
```

Dans une liste, dessinez `filterBox("##find", filter)` puis les lignes correspondantes. La fonction indique si la requête a changé. `FilterOptions` précise l’invitation traduite, la largeur en pixels écran, le raccourci et le focus/réinitialisation à l’apparition. Une largeur nulle occupe la ligne.

Ctrl+F (Cmd+F avec le comportement macOS d’ImGui) cible le filtre de la vue focalisée sans voler la saisie d’un autre éditeur de texte. L’hôte peut appeler `filter.focus()` depuis son routage de raccourcis. Échap dans le champ ou son bouton d’effacement vide la requête. Les widgets hors des conditions de filtrage restent utilisables.

`matches` exige tous les mots séparés par des espaces, répartis entre le libellé anglais visible, sa traduction et les mots-clés. L’ordre est libre. Casse latine/ASCII, accents, ligatures et accents combinés sont normalisés : `rugosite` trouve `Rugosité` et `roughness` trouve la même propriété. Les autres textes UTF-8 sont conservés ; il ne s’agit pas d’un repli Unicode complet. Ajoutez vous-même les mots-clés traduits et les chemins utiles. Les identifiants cachés `##` et `###` ne sont pas recherchés.

## Sections repliables et Inspector {#collapsible-sections-and-inspector}

```cpp
cyui::InspectorOptions options{&filter, "Surface"};
const bool any = std::any_of(fields.begin(), fields.end(), [&](const auto& field) {
    return cyui::propertyMatches(filter, field, "Surface");
});
if (cyui::beginFilteredSection("Surface", filter, any)) {
    for (const auto& event : cyui::inspector("##surface", state, fields, revision,
                                            validator, options)) {
        handlePropertyEdit(event);
    }
    cyui::endSection();
}
```

Les sections correspondantes s’ouvrent temporairement et retrouvent leur état replié après effacement. Incluez les noms des ancêtres dans le contexte des lignes. Les propriétés gardent identifiants, brouillons et validation ; masquer une édition active émet une proposition d’annulation. Passez le modèle complet à Inspector plutôt qu’un sous-ensemble.

La recherche examine noms, aide traduite et contexte, pas les valeurs ni mots de passe. Si l’hôte omet entièrement l’appel d’un panneau/section, il annule lui-même ses aperçus actifs. Effacer le filtre ne modifie jamais le modèle.

## Menus recherchables {#searchable-menus}

```cpp
cyui::TextFilter menuFilter; // persistent with this menu's state
std::vector<cyui::FilterMenuItem> entries{
    {1, "Save", "Ctrl+S"},
    {0, "Export", {}, {}, true, false, {
        {2, "Export PNG", {}, "image texture"},
        {3, "Export HDR", {}, "image", false}}}
};
if (ImGui::BeginMenu(cyui::trLabel("File").c_str())) {
    const auto action = cyui::filterMenu("##file", menuFilter, entries);
    if (action) dispatchAction(action);
    ImGui::EndMenu();
}
```

Un élément avec enfants est un sous-menu ; un élément sans nom/enfants et d’ID nul est un séparateur. Les actions ont des IDs 64 bits uniques et non nuls. L’hôte possède les états coché/activé et exécute l’ID retourné ; le filtre n’accède pas aux fichiers et ne lance aucune commande.

Sans requête, le menu conserve son arborescence. Une recherche montre les descendants avec leur chemin traduit. Nom/mots-clés du parent peuvent correspondre aux enfants et les mots peuvent se répartir entre chemin et feuille. Un parent désactivé conserve ses résultats désactivés. `FilterMenuOptions` règle la largeur en unités de conception, la remise à zéro et le focus à l’ouverture ; désactivez ces deux derniers pour un panneau incorporé. `filterMenuMatches` produit un instantané sans dessin.

## Python {#python}

```python
import cyui as c
from cyui_modules.inspector import inspector

query = c.TextFilter()  # persistent
c.panel_header(c.tr('Properties'), filter_state=query)
for key, edit in inspector('details', state, properties, filter_state=query,
                            filter_context='Surface'):
    handle_edit(key, edit)

entries = [c.FilterMenuItem(1, 'Save', 'Ctrl+S')]
# Inside an open imgui menu or popup:
action = c.filter_menu('file', menu_query, entries)
```

`cyui_modules.filters` fonctionne sans ImGui pour la recherche seule. Les noms sont `filter_box`, `begin_filtered_section`, `filter_menu`, `filter_menu_matches`, `fold_filter_text`. Inspector reçoit `filter_state` et `filter_context`. L’hôte applique et annule les éditions comme en C++.

Pour de grands modèles, mettez les correspondances en cache par requête/langue/révision et clippez les lignes visibles. Il n’existe pas d’index global automatique. Le filtre natif ne réécrit pas le contenu HTML de CEF. Les pages Filtering (21) et Details (12) de la galerie C++, ainsi que la galerie Python, illustrent ces API. Les panneaux Unreal peuvent appeler les mêmes fonctions du Core.

```image
{"id": "filtering-view", "src": "assets/filtering.webp", "alt": "Menus et propriétés filtrés", "controls": "minimal"}
```
