---
id: integration
title: Intégrer CyUI
description: Installer les dépendances, sélectionner les modules et initialiser une interface.
section: Découvrir
order: 1
sourceRevision: 2
---

CyUI ajoute des thèmes et des composants cohérents à Dear ImGui. Conservez une seule instance compatible d’ImGui, votre moteur de rendu et vos modèles métier.

Les textes de l’interface utilisent l’anglais par défaut et le français comme seconde langue intégrée. Utilisez `trLabel` pour les contrôles, fenêtres et onglets, et `tr` pour le texte affiché. Enregistrez les messages de votre application et proposez un choix de langue. Les chemins, identifiants et données utilisateur ne sont pas traduits. Voir [la localisation](doc:localization).

Prérequis : Dear ImGui **1.92 ou ultérieur**, branche docking recommandée, et C++17. CodeEditor et l’adaptateur CEF optionnel demandent C++20.

### CMake {#cmake}

Nommez la cible ImGui déjà utilisée par votre application avant `add_subdirectory`, puis liez les cibles CyUI nécessaires :

```cmake
set(CYUI_IMGUI_TARGET <the app's imgui target>)   # e.g. cydaw_imgui
add_subdirectory(external/CyUI ${CMAKE_BINARY_DIR}/cyui)
target_link_libraries(<app> PRIVATE CyUI::CyUI)
```

`CyUI::CyUI` est la cible de compatibilité regroupant les modules sélectionnés. Pour une intégration sélective, utilisez `<cyui/Core.h>` et `CyUI::Core`. Audio et Gallery gardent historiquement leur option activée ; désactivez-les explicitement avec la démo lorsqu’ils ne sont pas nécessaires. Core ne les lie jamais. Consultez [les dépendances exactes](doc:modules).

Exemple pour une application de lecture et comparaison de code :

```cmake
set(CYUI_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_GALLERY OFF CACHE BOOL "" FORCE)
set(CYUI_MODULES "Code;Diff" CACHE STRING "" FORCE)
# add_subdirectory(...) after these options
target_link_libraries(app PRIVATE CyUI::Diff)
```

### Sans CMake {#without-cmake}

Ajoutez `include/` aux répertoires d’inclusion. Compilez `src/Theme.cpp`, `src/Widgets.cpp`, `src/Layout.cpp` et `src/Filter.cpp` avec les sources ImGui de l’application. Audio ajoute `src/modules/Audio.cpp` ; Gallery ajoute `src/Gallery.cpp`. Définissez `CYUI_HAS_AUDIO=1` pour les exemples audio de la galerie. Les autres modules et leurs dépendances sont décrits dans [la sélection des modules](doc:modules#select-only-what-an-application-needs).

### Initialisation {#startup}

Après `ImGui::CreateContext()` et avant le premier `NewFrame`, remplacez l’ancienne initialisation du thème par :

```cpp
#include <cyui/CyUI.h>

cyui::Config config;
config.themeKey = "mid-grey";            // see the theme table below
config.density  = cyui::Density::compact; // or comfortable
config.dpiScale = dpiScale;              // 1.0 at 96 dpi
cyui::apply(config);
cyui::loadFonts();                       // Segoe UI + Semibold + JetBrains Mono + MDL2 icons
```

Lors d’un changement de DPI, appelez `applyDpi(newScale)` ; une échelle inchangée ne déclenche rien. `apply` et un changement effectif de DPI reconstruisent l’intégralité d’`ImGuiStyle` : appliquez ensuite vos éventuels réglages supplémentaires. Appelez `renderNotifications()` une fois en fin de frame pour afficher les notifications.

`Config::squareWindows` produit des fenêtres carrées et opaques adaptées aux fenêtres OS et éditeurs de plugins. `FontConfig::size=0` prend la taille de la densité ; `fallbackFolder` permet de fournir Inter ou Roboto en `.ttf` lorsque les polices d’interface Windows sont absentes. `monoFont()` est JetBrains Mono Regular 2.304, embarquée sous SIL OFL 1.1 ; distribuez `third_party/jetbrains-mono/OFL.txt` avec l’application. Aucune installation système de police n’est requise pour le code. Code/CodeEditor/Diff natifs proposent Ctrl + molette par vue ; consultez le guide du code pour textScale et wheelZoom. `icons` active la fusion de Segoe MDL2 Assets quand cette police est disponible. Distribuez vos polices de repli avec leurs droits appropriés.

Chaque contexte ImGui possède sa palette, son thème, sa densité, ses DPI, ses polices, ses saisies numériques et ses notifications. Initialisez chaque contexte avec `apply` et `loadFonts` lorsqu’il est courant. Les appels de thème affectent ce contexte. Sans contexte courant, `setPalette`, `setTheme` et `setDensity` changent les contextes existants et les valeurs par défaut des futurs contextes ; aucun autre thread ne doit alors dessiner. Sans contexte courant, `apply` et `applyDpi` ne changent que les valeurs par défaut. Les états sont libérés à la destruction du contexte.

| Clé | Apparence |
| --- | --- |
| `mid-grey` | Gris moyens, accent orange, thème par défaut |
| `light-grey` | Gris clairs, texte sombre, accent orange |
| `amber-night` | Gris bleutés profonds, accent ambre |
| `violet` | Ardoise sombre, accent violet, information cyan |
| `navy` | Bleu marine profond, accent bleu, rendu plat |

La densité `compact` convient aux éditeurs et outils denses ; `comfortable` aux lanceurs et panneaux de réglages.
