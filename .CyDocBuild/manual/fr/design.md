---
id: design
title: Thèmes, disposition et migration
description: Appliquer les rôles de palette, les dimensions DPI et les composants adaptés.
section: Guides
order: 11
sourceRevision: 1
---

Choisissez chaque couleur dans `cyui::palette()` et chaque dimension dans `cyui::dp()` ou le style. Une interface cohérente utilise les mêmes règles dans les widgets standards et les dessins personnalisés.

| Rôle | Usage |
| --- | --- |
| `bg0` | Fond profond des canevas, timelines et barres d’état |
| `bg1` | Fond des panneaux |
| `bg2` | En-têtes, cartes et fenêtres contextuelles |
| `bg3`, `bg4`, `bg5` | Contrôle au repos, survolé, pressé/actif |
| `line`, `lineStrong` | Traits et séparateurs |
| `text`, `textDim`, `textFaint` | Texte principal, secondaire et indications |
| `accent`, `accentHover`, `accentDim`, `accentText`, `selection` | Action principale, activité et sélection |
| `success`, `warning`, `error`, `info` | Couleurs sémantiques |

Utilisez `alpha`, `mix`, `brighter` et `darker` pour les variantes, `categorical(i)` pour les séries. Gardez un accent principal et un rendu plat. Pour les tailles, préférez aussi `GetFrameHeight`, `GetFontSize` et `GetStyle`.

Les titres utilisent `heading(texte, 1|2|3)`, les groupes `sectionTitle`, les petites étiquettes `overline`. `textDim` et `textFaint` créent les niveaux secondaires. Évitez de fabriquer un titre avec `Text` et une couleur poussée.

Une vue possède une action principale (`primaryButton`) ; utilisez `button` pour les autres actions, `ghostButton`/`iconButton` dans les barres d’outils et `dangerButton` pour les suppressions. Chaque bouton d’icône doit avoir une infobulle indiquant son action et son raccourci. Séparez les groupes par `toolbarSeparator`.

Présentez les réglages dans `beginProperties/property/endProperties`, au sein de sections repliables. Groupez les contenus dans des cartes ; réservez les tons accent/avertissement/erreur aux situations qui demandent de l’attention. Un panneau vide explique l’action possible avec `emptyState`.

Signalez les événements avec `notify`, les conditions persistantes avec `callout`, les tâches avec `progressBar` ou `spinner` et les états avec `badge` ou `statusDot`. Les infobulles `tooltip` sont retardées et reviennent à la ligne ; `helpMarker` explique une option.

Conservez les contrôles ImGui standards lisibles : Checkbox, InputText, ColorEdit, tableaux, arbres, menus, onglets et DragFloat2 reçoivent le thème. Préférez `ScopedColor`, `ScopedVar`, `ScopedFont`, `ScopedID` et `ScopedDisabled` aux piles Push/Pop manuelles. N’appelez pas `StyleColors*` après `apply` ; changez la palette avec `setPalette`. Utilisez `ScopedFont` plutôt que `SetWindowFontScale`, et une identité stable `Label###stableId` pour un libellé variable.

## Migrer une interface ImGui {#migrate-an-imgui-interface}

Procédez panneau par panneau en conservant les identifiants, la logique et les comportements.

| Ancien code | Remplacement |
| --- | --- |
| Fonction de thème, `StyleColorsDark`, couleurs de style | `apply(config)` puis `loadFonts()` |
| Couleurs `IM_COL32` ou `ImVec4` du dessin UI | Rôle de `palette()` |
| Taille fixe en pixels | `dp(valeur)` |
| Bouton principal / suppression | `primaryButton` / `dangerButton` |
| Petit bouton de barre d’outils | `iconButton(icon, tooltip, selected)` |
| Checkbox de réglage | `toggleSwitch` ; conserver Checkbox dans les listes |
| Rangée de boutons de mode | `segmented` |
| Texte de section et séparateur | `sectionTitle` ou `beginSection` |
| Texte gris personnalisé | `textDim` / `textFaint` |
| Label, SameLine puis champ | Table `beginProperties` et `property` |
| Curseur dans un inspecteur | `sliderBar`, ou conserver SliderFloat |
| Champ de recherche | `searchField` |
| BeginChild utilisé comme carte | `beginCard/endCard` |
| Barre de progression | `progressBar` |
| Confirmation modale | `beginModal/modalButtons` |
| Message d’état ponctuel | `notify` |
| Barres de fenêtre positionnées manuellement | `beginTopBar`, `beginStatusBar`, `beginSidebar` |
| Contrôles audio dessinés à la main | `knob`, `fader`, `meter` du module Audio |

Les vues personnalisées peuvent conserver leurs dessins : fond `bg0`, grille `grid/gridStrong`, sélection `selection` bordée d’`accent`, texte `text/textDim`, poignées `bg5` ou `accent`. Ouvrez `showGallery()` depuis un menu de développement et contrôlez les cinq thèmes ainsi que les deux densités pour détecter les contrastes incorrects.

## Comparer les thèmes {#themes}

```gallery
{"id": "themes", "controls": "minimal", "items": [{"type": "image", "src": "assets/overview.webp", "alt": "Graphite et orange", "title": "Graphite et orange", "description": "Même galerie dans un autre thème."}, {"type": "image", "src": "assets/theme-light.webp", "alt": "Gris clair", "title": "Gris clair", "description": "Même galerie dans un autre thème."}, {"type": "image", "src": "assets/theme-amber.webp", "alt": "Nuit ambre", "title": "Nuit ambre", "description": "Même galerie dans un autre thème."}, {"type": "image", "src": "assets/theme-violet.webp", "alt": "Violet", "title": "Violet", "description": "Même galerie dans un autre thème."}, {"type": "image", "src": "assets/theme-navy.webp", "alt": "Bleu marine", "title": "Bleu marine", "description": "Même galerie dans un autre thème."}]}
```
