---
id: modules
title: Sélectionner les modules
description: Cibles CMake, dépendances et contrats des composants optionnels.
section: Guides
order: 10
sourceRevision: 3
---

CyUI 1.2 fournit un socle Dear ImGui et des modules d’interface sélectionnables. Le code CyUI est sous MIT. L’application conserve modèles, moteurs, fichiers, services, validation et historique Undo.

## Localisation dans chaque intégration {#localization-in-every-integration}

Core inclut l’anglais par défaut, le français et des catalogues extensibles. Gardez les identifiants stables avec des libellés traduits. [La localisation](doc:localization) décrit C++, Python, Web, Unreal, polices et responsabilités de l’hôte.

## Cibles et dépendances {#targets-and-dependencies}

Core fournit thèmes, localisation, filtres rapides, polices, icônes, widgets ordinaires, disposition et notifications. Les autres modules le lient automatiquement. Le filtrage est optionnel par vue avec TextFilter, filterBox, filterMenu et l’en-tête de panneau filtré. Inspector accepte un filtre sans dépendance supplémentaire.

Les en-têtes ci-dessous sont relatifs à `include/cyui/`. Chaque cible porte le préfixe `CyUI::`.

| Cible | En-tête | Présentation | Dépendance CyUI |
| --- | --- | --- | --- |
| Audio | `modules/Audio.h` | Potentiomètres, faders, crête/RMS | Core |
| Canvas | `modules/Canvas.h` | Coordonnées doubles, grille, clipping, déplacement/zoom | Core |
| Plots | `modules/Plots.h` | Axes linéaires/log, lignes, marches, points, barres, histogrammes, cartes de chaleur | Canvas |
| Curves | `modules/Curves.h` | Clés, Bézier, sélection, alignement, copier/coller | Plots |
| Nodes | `modules/Nodes.h` | Cartes, ports, liens et propositions de déplacement/connexion/suppression | Canvas |
| Timeline | `modules/Timeline.h` | Pistes, clips, marqueurs, règle, déplacement/rognage, plage | Canvas |
| AudioViews | `modules/AudioViews.h` | Formes d’onde, spectre, spectrogramme, LUFS, crête vraie, corrélation, EQ/phase et dynamique | Plots |
| Code | `modules/Code.h` | Contrats/constructeurs de fragments paginés ; Lecture de source, 102 syntaxes, registre, superpositions sémantiques, annotations, recherche | Core + Lexilla + threads système |
| Diff | `modules/Diff.h` | Comparaison unifiée/côte à côte, numéros, blocs et différences internes | Code |
| CodeEditor | `modules/CodeEditor.h` | TextEditor en mémoire et PagedCodeEditor fourni par l’hôte (capacités distinctes) | Code + TextEditor MIT, C++20 |
| Inspector | `modules/Inspector.h` | Propriétés typées, int64 exact, validation et brouillons | Core |
| Assets | `modules/Assets.h` | Dossiers hiérarchiques, grille/liste, recherche, vignettes, glisser-déposer | Core |
| Images | `modules/Images.h` | Texture, damier, recadrage et dégradé | Canvas |
| Viewport | `modules/Viewport.h` | Surface rendue par l’hôte, caméra orbitale et transformation 2D | Images |
| Commands | `modules/Commands.h` | Palette de commandes recherchable au clavier | Core |
| Diagnostics | `modules/Diagnostics.h` | Journaux bornés synchronisés, filtres et navigation | Core + threads système |
| DocumentViews | `modules/DocumentViews.h` | Sous-ensemble Markdown, code, liens, tables et conversations en cache | Code |
| Web | `modules/Web.h` | Contrat texture/entrée, origines, messages et thème | Core |
| Gallery | `Gallery.h` | Exemples et éditeur de thème | Modules activés uniquement |

`CyUI::Code::Diff` est un alias de `CyUI::Diff`. `CyUI::CyUI` regroupe les modules sélectionnés. Un module est une bibliothèque statique, pas un plugin chargé dynamiquement.

Adaptateurs externes optionnels : `CyUI::ImGuizmo` utilise `CYUI_IMGUIZMO_TARGET` fourni par l’hôte avec Viewport et `adapters/ImGuizmo.h`. `CyUI::WebCEF` utilise Web, `CYUI_BUILD_WEB_CEF=ON`, un SDK/runtime CEF externe et C++20 avec `adapters/WebCEF.h`. `All` ne les active ni ne les télécharge. Web accepte un backend de l’hôte ; l’implémentation fournie est CEF. Voir [son cycle de vie et sa distribution](doc:web).

WebCEF fournit un callback `release` de texture, des événements `external` portant userGesture/mainFrame/sourceOrigin limités par externalSchemes et des événements `dropped` comptant les rejets. Le bridge/helper fournis ne sont pas sandboxés sous Windows.

Le [module Unreal Editor](doc:unreal) se prépare séparément avec `tools/package_unreal.py --layout module`. Il s’intègre dans Source/CyUI d’un plugin ou un fournisseur partagé, sans menu ni fenêtre de démo automatique. Ce n’est pas une cible CMake ni une dépendance de Core. Utilisez un nom unique `--module-name` dans un plugin redistribué, ou le fournisseur commun.

## Sélectionner uniquement le nécessaire {#select-only-what-an-application-needs}

```cmake
set(CYUI_IMGUI_TARGET app_imgui)
set(CYUI_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_GALLERY OFF CACHE BOOL "" FORCE)
set(CYUI_MODULES "Code;Diff;Inspector" CACHE STRING "" FORCE)
add_subdirectory(external/CyUI)
target_link_libraries(app PRIVATE CyUI::Diff CyUI::Inspector)
```

```cpp
#include <cyui/Core.h>
#include <cyui/modules/Diff.h>
#include <cyui/modules/Inspector.h>
```

Les dépendances sont automatiques : Diff active Code, sans CodeEditor, audio, tracés ou navigateur. Les cibles explicites limitent macros et dépendances publiques ; `CYUI_HAS_CODE`, `CYUI_HAS_DIFF`, etc. appartiennent à leurs cibles.

`CYUI_MODULES` accepte une liste séparée par points-virgules, Core, une liste vide ou All. La démo autonome complète prend All par défaut. Les intégrations et builds sans démo n’activent pas de nouveaux modules optionnels par défaut. Audio et Gallery gardent leur ancien défaut ON : désactivez-les explicitement. Core ne les lie jamais. Les anciennes déclarations Widgets.h/CyUI.h restent compatibles.

Sans CMake, compilez les quatre sources Core et les `src/modules/<Name>.cpp` sélectionnés avec leurs dépendances et macros `CYUI_HAS_<NAME>` pour Gallery/CyUI.h. CodeEditor ajoute `src/modules/PagedCodeEditor.cpp`, TextEditor.cpp, son include et C++20. Code et ses dépendants ajoutent `src/modules/code/*.cpp`, Lexilla lexlib/lexers et les includes Lexilla/Scintilla. Définissez `CYUI_CODE_LANGUAGE_CATALOGUE=1` pour le catalogue étendu. `build_demo.bat` compile la démo Windows complète.

## Contrat commun des données et éditions {#shared-data-and-edit-contract}

- Attribuez à chaque objet un ID uint64 stable et non nul. Les résultats d’action utilisent zéro pour aucun ; les gestes peuvent toutefois suivre un objet d’ID nul grâce à leurs drapeaux. Gardez un état par vue.
- Déplacements/zooms préservent les plages : un débordement logarithmique ou un intervalle inférieur à la résolution relative `abs(value) × 1e-12` est ignoré. Les sélections retournent min ≤ max.
- ArrayView et callbacks empruntent leurs données pendant l’appel. Les points de courbe sont finis et triés ; les durées sont positives. Unités, règles de connexion, interpolation et alignement appartiennent à l’hôte.
- Les `Edit<T>` proposent begin/update/commit/cancel. Une action atomique peut directement commit. L’aperçu ne change pas le modèle ; validez et appliquez un commit en une transaction Undo. Incrémentez la révision lors d’un remplacement externe pour annuler un geste devenu obsolète.
- Préparation et dessin se font sur le thread UI. LogBuffer append/clear/snapshot sont synchronisés et acceptent des producteurs concurrents.
- Couleurs UI par palette, dimensions par dp/style. Les couleurs de dégradé, textures, mesures et matrices restent des données applicatives.
- Aucun module ne lit des projets, n’exécute Git/un terminal, ne décode audio/vidéo, ne calcule de FFT ou ne lance de serveur de langage. Les actions de presse-papiers sont des requêtes locales. Un navigateur n’est créé que par une intégration Web explicite.

Préparez les modèles quand leurs données changent et ne dessinez que des jeux bornés/visibles. Plots accepte par défaut 65 536 points par série et signale les IDs trop grands dans `skippedSeries` ; utilisez reducePlot ou un index hôte. Heatmap borne chaque axe à 256 cellules. Diff combine ancres patience et LCS borné (4 millions de cellules par défaut) ; `approximate()` indique un remplacement complet de région. Les deux sources sont conservées ; CRLF et LF sont égaux avec une notification de différence de fins de ligne. Source et Markdown sont bornés à 32 Mio ; l’hôte pagine les grands fichiers. Les journaux ont par défaut 10 000 lignes / 8 Mio. Waveform consomme des enveloppes min/max préparées.

## Python {#python}

```sh
python -m pip install -e ".[ui]"
```

`cyui` conserve l’API de thème/widgets. `cyui_modules` propose audio, canvas, plots, curves, nodes, timeline, audioviews, code, diff, inspector, assets, images, viewport, commands, diagnostics, documents (alias documentviews), web et cef. Les modèles s’importent sans charger imgui-bundle ; le dessin le charge à la demande. L’installation déclare imgui-bundle comme dépendance UI ; aucun NumPy, décodeur ou moteur 3D n’est nécessaire.

IDs, plages, palette, phases d’édition, origines et diff borné suivent les mêmes contrats. CodeEditor Python est une saisie multiligne ImGui avec Undo natif et aperçu syntaxique, sans prétendre exposer multicurseurs/complétion du backend C++. `CEFRuntime` charge explicitement la bibliothèque C ABI et le helper ; le paquet Python n’embarque pas CEF. Voir [Python](doc:python) et [Web](doc:web).

```image
{"id": "modules-view", "src": "assets/assets.webp", "alt": "Navigateur de contenu avec dossiers et vignettes", "controls": "minimal"}
```

## Documents paginés {#paged-documents}

`CodePaging.h` appartient à Code (C++17) ; `PagedCodeEditor.h` à CodeEditor
(C++20). [Le guide fournisseur](doc:paged-code) décrit lectures bornées, positions
UTF-16, syntaxe contextuelle et transactions possédées par l’hôte. L’éditeur
paginé ne reprend pas la limite d’instantané de 32 Mio du lexer classique et ne
copie pas le fichier entier. Les budgets de pages et de saisie restent actifs.
Cette API C++ native n’a pas encore de liaison Python ou Web. Le packaging source
l’inclut avec CodeEditor ; compilation Unreal Editor et saisie de chaque plateforme
restent à valider par l’hôte.
