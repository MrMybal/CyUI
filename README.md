# CyUI : l'interface Dear ImGui des logiciels Cy*

CyUI est une library C++17 qui se pose **par-dessus Dear ImGui (1.92 ou plus)**.
Elle donne à n'importe quel logiciel ImGui le style de **CyDAW 2**, de
**Shaderixel** et de **CyEngineHub**, au lieu du look ImGui de base.

![mid-grey](docs/screenshots/mid-grey.png)

## Ce qu'il y a dedans

- **Thème** : une palette sémantique (fonds `bg0`..`bg5`, traits, 3 niveaux
  de texte, un seul accent, couleurs de sens) et deux densités (compact,
  confortable). Le DPI est géré, et `.cytheme` permet de sauver et charger
  ses propres thèmes. Les thèmes de CyDAW sont compatibles.
- **5 thèmes intégrés** :

  | Clé | Origine |
  |-----|---------|
  | `mid-grey` (par défaut) | CyDAW |
  | `light-grey` | CyDAW |
  | `amber-night` | CyDAW |
  | `violet` | Shaderixel |
  | `navy` | CyEngineHub |

- **Polices** : Segoe UI, Segoe UI Semibold, Consolas, avec les **icônes
  Segoe MDL2** fusionnées (`cyui::icon::*`, 104 icônes).
- **Widgets** :
  - boutons : primary, secondary, ghost, outline, danger, success
  - icon buttons et toggles
  - switch et segmented control
  - knob, fader et vu-mètre (comportement CyDAW)
  - slider-barre, champ de recherche
  - cartes, sections repliables, tables de propriétés (inspecteur)
  - badges, status dots, barres de progression, spinner
  - empty states, callouts, notifications toast, modales
- **Layout** : top bar, status bar, sidebar de navigation façon CyEngineHub,
  dock space, toolbars.
- **Galerie** : `cyui::showGallery()` montre tout, avec un sélecteur de thème.
  `cyui::showThemeEditor()` édite la palette en direct.

| navy (CyEngineHub) | violet (Shaderixel) | light-grey (CyDAW) |
|---|---|---|
| ![](docs/screenshots/navy.png) | ![](docs/screenshots/violet.png) | ![](docs/screenshots/light-grey.png) |

## Utilisation rapide

```cpp
#include <cyui/CyUI.h>

// après ImGui::CreateContext()
cyui::Config config;
config.themeKey = "mid-grey";
config.dpiScale = dpi;
cyui::apply(config);
cyui::loadFonts();

// dans la frame
if (cyui::primaryButton("Exporter")) export();
cyui::iconButton(cyui::icon::play, "Lecture (Espace)", playing);
cyui::knob("##gain", &gain, 0, 1, 0.5f, { 32, 0, "Gain" });
cyui::renderNotifications(); // en fin de frame
```

Avec CMake :

```cmake
set(CYUI_IMGUI_TARGET ma_target_imgui)
add_subdirectory(external/CyUI cyui)
target_link_libraries(mon_app PRIVATE CyUI::CyUI)
```

### Choisir les modules

`CyUI::Core` contient les thèmes, widgets généraux et layouts.
`CyUI::Audio` ajoute les knobs, faders et vu-mètres. `CyUI::Gallery` contient
la galerie et l'éditeur de thème. Chaque cible peut être liée séparément.
`CyUI::CyUI` conserve l'intégration complète existante.

Pour une application sans audio ni galerie, désactive `CYUI_BUILD_AUDIO`,
`CYUI_BUILD_GALLERY` et `CYUI_BUILD_DEMO`, inclus `<cyui/Core.h>` et lie
`CyUI::Core`. Les options Audio et Gallery sont activées par défaut.

Le [guide des modules](docs/MODULES.md) décrit les cibles disponibles et le
découpage proposé après examen des interfaces des applications Cy* : canvas,
plots, courbes, nœuds, timelines, code, différences, assets et viewports.

## Pour les IA

Quand tu demandes à une IA de refaire l'interface d'un logiciel, dis-lui :

> Utilise la library CyUI du dossier `external/CyUI`. Lis
> `docs/AI_GUIDE.md` et applique-le à l'interface ImGui de ce projet.

Le guide contient :
- l'intégration
- les règles de style (pas de couleurs ni de tailles en dur, un seul bouton
  primaire, des tables de propriétés...)
- une **table de migration ImGui → CyUI**
- l'API complète et des recettes

## Démo

```bash
build_demo.bat
```

`build_demo.bat` compile `build/cyui_demo.exe` avec MSVC 2022. La démo
montre une app complète : barre de menus, barre du haut, sidebar, dock,
inspecteur, galerie et toasts.

La démo utilise une copie de Dear ImGui 1.93 WIP (docking) placée dans
`third_party/imgui`. Les apps gardent leur propre ImGui.

## Arborescence

```
include/cyui/   Core.h, CoreWidgets.h, modules/Audio.h, CyUI.h (compatibilité)
src/            Theme.cpp, Widgets.cpp, Layout.cpp, Gallery.cpp, modules/Audio.cpp
examples/       demo_win32_dx11/main.cpp
docs/           AI_GUIDE.md, MODULES.md, screenshots/
third_party/    imgui (pour la démo seulement)
```

## Développement

Pour activer le contrôle Gitleaks avant chaque commit, installe
[Gitleaks](https://github.com/gitleaks/gitleaks/releases) et active le hook
dans ton clone :

```bash
git config --local core.hooksPath .githooks
```

Le hook utilise `gitleaks` dans le `PATH`, ou la copie locale dans
`build/tools/gitleaks/gitleaks.exe`.
