# CyUI : l'interface Dear ImGui des logiciels Cy*

CyUI est une bibliothèque **MIT**, utilisable dans des projets commerciaux.
Elle vise Windows, Linux et macOS avec le backend Dear ImGui fourni par l'application.
Les validations locales de cette version ont été effectuées sous Windows ; la
[matrice de plateformes](docs/WEB.md) précise les vérifications restantes.

CyUI est une library C++17 (C++20 pour CodeEditor et CEF) qui se pose **par-dessus Dear ImGui (1.92 ou plus)**.
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

`CyUI::Core` fournit le socle. Les modules disponibles sont **Audio, Canvas,
Plots, Curves, Nodes, Timeline, AudioViews, Code, Diff, CodeEditor, Inspector,
Assets, Images, Viewport, Commands, Diagnostics, DocumentViews et Web**.
Chaque application lie seulement les cibles utiles ; les dépendances entre modules
sont résolues automatiquement. La galerie et les adaptateurs ImGuizmo/CEF sont optionnels.

```cmake
set(CYUI_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_GALLERY OFF CACHE BOOL "" FORCE)
set(CYUI_MODULES "Code;Diff;Inspector" CACHE STRING "" FORCE)
add_subdirectory(external/CyUI)
target_link_libraries(mon_app PRIVATE CyUI::Diff CyUI::Inspector)
```

`CyUI::CyUI` conserve l'API globale et lie les modules sélectionnés. Les contrôles
Audio et Gallery gardent leurs options ON pour les intégrations existantes ; le
nouveau sélecteur `CYUI_MODULES` est vide par défaut dans une application hôte.
Le [guide des modules](docs/MODULES.md) décrit toutes les API, dépendances et
frontières avec les moteurs et documents propres à chaque application.

### Python et Web

```sh
python -m pip install -e ".[ui]"
```

`cyui` conserve le thème et les widgets généraux. `cyui_modules` ajoute les modules
facultatifs en Python, avec des modèles importables sans moteur audio, vidéo ou 3D.
Un [exemple de galerie Python](examples/python_modules.py) montre les interactions.

Le module **Web** peut afficher une interface HTML/JavaScript issue de TypeScript,
avec **CEF** comme moteur commun et un rendu dans une texture ImGui. CEF reste
une dépendance explicite, distribuée séparément pour chaque OS/architecture.
Le pont JavaScript/TypeScript et l'intégration iframe CyMD sont fournis dans `web/`.
Le [guide Web](docs/WEB.md) précise les exemples C++/Python, les échanges de messages,
les services nécessaires et le packaging Linux/macOS à valider.

Le code CyUI reste MIT. Les dépendances conservent leurs propres licences et
notices ; voir [licences](docs/LICENSING.md) et [notices tierces](THIRD_PARTY_NOTICES.md).
Les applications examinées ne sont pas incluses ni modifiées.

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
include/cyui/   Core.h, Interaction.h, modules/, adapters/, CyUI.h
src/            Theme.cpp, Widgets.cpp, Layout.cpp, Gallery.cpp, modules/, adapters/
python/         cyui.py, cyui_modules/
web/            pont JavaScript/TypeScript et variables CSS
examples/       demo_win32_dx11/, cef_helper/, web/, python_modules.py
tests/          modèles, gestes, modules isolés, navigateur, captures
docs/           AI_GUIDE.md, MODULES.md, WEB.md, LICENSING.md, screenshots/
third_party/    imgui, texteditor (avec leurs licences)
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
