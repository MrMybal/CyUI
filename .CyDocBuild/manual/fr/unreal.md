---
id: unreal
title: Module réutilisable pour Unreal Editor
description: Intégrer CyUI dans un plugin existant ou un fournisseur commun.
section: Intégrations
order: 35
sourceRevision: 1
---

CyUI propose un **module Editor optionnel** avec bibliothèque CyUI/ImGui, rendu Slate, entrées, presse-papiers, textures de police et registre de textures Unreal. Le chargement n’ajoute ni menu, ni démo, ni fenêtre. Le plugin propriétaire gère disposition, commandes et état. Core/CMake/Python ne dépendent pas d’Unreal. L’intégration concerne les outils Editor C++ natifs, pas UMG, le gameplay ni Blueprints.

## Incorporer le module dans un plugin {#embed-the-module-in-an-existing-plugin}

Préparez un nouveau répertoire avec Python 3.10+ :

```sh
python tools/package_unreal.py --layout module --output build/unreal-module/CyUI --modules Core Plots Inspector Code Diff
```

Copiez le dossier CyUI unique dans `<YourPlugin>/Source/CyUI`. Il contient Build.cs, en-têtes publics, backend, bibliothèque et dépendances sélectionnées, notices MIT. Aucun plugin supplémentaire ni chemin vers un checkout externe n’est nécessaire. Ajoutez à Modules du .uplugin, sans remplacer les entrées existantes :

```json
{ "Name": "CyUI", "Type": "Editor", "LoadingPhase": "PostEngineInit" }
```

Ajoutez la dépendance de votre module **Editor** :

```csharp
PrivateDependencyModuleNames.Add("CyUI");
CppStandard = CppStandardVersion.Cpp20;
```

Utilisez PublicDependencyModuleNames si vos en-têtes publics exposent les types CyUI. Un module runtime/gameplay ne doit pas dépendre de cet Editor. Régénérez les projets IDE et compilez la cible Editor par UnrealBuildTool.

Les implémentations Library/src et tierces gardent leurs octets en `.inl`. Les ponts `.cpp` les compilent une seule fois, sans découverte récursive en double par UBT. API et includes restent identiques. Library/SOURCE_LAYOUT.json décrit les chemins ; le manifeste TextEditor utilise le nom empaqueté. Aucun code moteur n’est copié. CYUI_API reste vide en CMake statique.

### Nom unique pour un plugin redistribué {#a-plugin-you-redistribute-unique-module-name}

Deux plugins actifs ne peuvent pas définir tous deux un module CyUI. Utilisez un fournisseur partagé ou un nom propre :

```sh
python tools/package_unreal.py --layout module --module-name MyToolCyUI --output build/unreal-module/MyToolCyUI --modules Core Plots
```

Copiez dans Source/MyToolCyUI, déclarez MyToolCyUI dans le descriptor et Build.cs. Le script renomme classe/fichier Build.cs, dossier, descriptor et noms de tests, définit CYUI_UNREAL_MODULE pour IMPLEMENT_MODULE/FCyUIModule::Get, et mappe CYUI_API vers MYTOOLCYUI_API. Les classes et en-têtes C++ restent identiques.

Chaque copie possède son ImGui exporté avec IMGUI_API=CYUI_API et un chemin include public. Un consommateur dépend d’un seul module CyUI et n’utilise pas les headers d’un autre plugin ImGui. ImGui n’est pas renamespacé : les DLL Windows isolent leurs symboles ; la résolution entre copies sous Linux/macOS n’est pas validée.

## Partager un fournisseur entre plugins {#share-one-module-between-several-plugins}

Une seule définition du module CyUI doit exister dans le projet. Préparez le conteneur fournisseur :

```sh
python tools/package_unreal.py --layout plugin --output build/unreal-provider/CyUI --modules All --gallery
```

Copiez-le dans `<Project>/Plugins/CyUI` et ajoutez à la liste Plugins de chaque consommateur :

```json
{ "Name": "CyUI", "Enabled": true }
```

Le Build.cs dépend toujours du **module** CyUI. Le plugin fournisseur est un conteneur sans interface automatique ; s’il a un autre nom, utilisez ce nom dans Plugins et le nom de module dans Build.cs. Respectez la hiérarchie projet/moteur Unreal.

`--modules Core` est minimal, All prend les modules ordinaires et résout leurs dépendances. `--gallery` ajoute seulement l’API galerie, dessinée à la demande de l’hôte. CEF et ImGuizmo externe ne sont pas empaquetés. Un dossier de sortie existant est refusé : préparez un staging neuf et remplacez l’ancien module éditeur fermé. Redémarrez après changement du backend/bibliothèque ; le déchargement dynamique du module partagé est désactivé.

## Exemple complet de consommateur {#complete-consuming-plugin-example}

`examples/unreal/CyUIToolExample` est un plugin distinct avec Build.cs, descriptor, entrée Window, onglet nomade, disposition Slate, dessin CyUI et nettoyage. Son descriptor référence le fournisseur CyUI. Il incorpore MakePanel, sans fenêtre CyUI autonome. L’exemple peut ouvrir la galerie si elle est sélectionnée. Pour votre plugin existant, reprenez ce motif et déclarez son module comme ci-dessus.

## Enregistrer un outil Editor {#register-an-editor-tool}

Dans le Build.cs du consommateur :

```csharp
PrivateDependencyModuleNames.AddRange(new[] { "Core", "CyUI" });
CppStandard = CppStandardVersion.Cpp20;
```

Enregistrez l’outil dans StartupModule et retirez-le dans ShutdownModule :

```cpp
#include "CyUIModule.h"
#include <cyui/Core.h>

void FMyEditorTools::StartupModule()
{
    FCyUIModule::Get().RegisterTool(
        "MyTools.Inspector", NSLOCTEXT("MyTools", "Inspector", "My CyUI Inspector"),
        FOnDrawCyUITool::CreateRaw(this, &FMyEditorTools::DrawInspector));
}

void FMyEditorTools::DrawInspector()
{
    cyui::heading("My editor tool");
    if (cyui::primaryButton("Inspect selection")) {
        // Read the editor selection / request a host action here.
    }
}

void FMyEditorTools::ShutdownModule()
{
    if (FCyUIModule::IsAvailable())
        FCyUIModule::Get().UnregisterTool("MyTools.Inspector");
}
```

RegisterTool refuse IDs vides/dupliqués et callbacks non liés. OpenTool(id) ouvre l’onglet enregistré ; le plugin ajoute son menu/bouton/commande. UnregisterTool détache le callback avant de demander la fermeture, empêchant un propriétaire déchargé de dessiner. MakePanel(callback) retourne un SWidget pour une disposition existante. Conservez son propriétaire pendant sa vie ; appelez Detach avant destruction d’un callback brut :

```cpp
#include "CyUIModule.h"
#include "SCyUIPanel.h"

// During your toolkit/tab construction, keep this as an owning-plugin member:
Panel = StaticCastSharedRef<SCyUIPanel>(FCyUIModule::Get().MakePanel(
    FOnDrawCyUITool::CreateRaw(this, &FMyEditorTools::DrawInspector)));
// Insert Panel.ToSharedRef() in a Slate slot or SDockTab content.
// Before unloading the owning plugin:
if (Panel) { Panel->Detach(); Panel.Reset(); }
```

CyUIModule.h expose FCyUIModule ; CyUIEditorModule.h / FCyUIEditorModule restent des noms compatibles. Les deux méthodes acceptent FOnInitializeCyUITool en dernier argument, exécuté une fois par panneau avec son contexte courant, après les polices par défaut et avant la frame. Fusionnez une police de l’hôte dans uiFont avec MergeMode/DstFont ou configurez des polices additionnelles. SCyUIPanel propose OnInitialize. UTF-8 ne fournit pas les glyphes manquants : chargez des polices couvrant les écritures et leurs droits de distribution.

Le connecteur possède CreateContext, NewFrame, Begin/End racine, Render et notifications. Le callback dessine le **contenu** : ne recréez pas de frame, ne changez/détruisez pas le contexte et ne chargez pas de police depuis le dessin. Gallery peut créer des fenêtres ImGui ordinaires internes. Fenêtres OS et docking appartiennent à Slate ; les multi-viewports ImGui OS sont désactivés.

Toutes les API et callbacks utilisent le game thread Editor. Les changements d’assets/monde passent par transactions, Modify, validation et Undo/Redo de l’outil. Un callback peut dessiner plusieurs fois par frame Editor : déclenchez les actions sur événement, et préparez les données coûteuses hors peinture.

Chaque panneau possède son contexte, ses saisies, sa molette, ses notifications, sa palette, densité, DPI et polices. Une modification de thème cible le contexte courant ; les nouveaux panneaux héritent des valeurs réglées sans contexte. Depuis Slate, utilisez `Panel->RunInContext([] { cyui::setTheme("navy"); })` et `Panel->Notify("Saved", Tone::success)` ou notify(context,...). Une notification sans contexte peut être abandonnée ; choisissez explicitement le panneau. Slate gère le DPI bureau, DisplayFramebufferScale la densité des glyphes ; CyUI garde le DPI logique 1 et ne restyle pas chaque peinture.

Chaque touche appartient à un côté : ImGui la reçoit lorsqu’un élément/champ/modal capture le clavier ou qu’une vue focalisée appelle SetNextFrameWantCaptureKeyboard(true), ainsi que les touches de navigation simples pendant sa navigation clavier. Les autres vont uniquement à Unreal : Ctrl+S/Z, Suppr, F7 ne s’exécutent pas deux fois. Un canevas avec ses propres raccourcis doit revendiquer le clavier de cette façon. Les caractères suivent la capture. La molette est verticale ; Shift la transforme horizontalement dans ImGui. Les atlas sont des UTexture2D transitoires mis à jour par rectangles modifiés.

## Afficher des textures Unreal {#show-unreal-textures}

```cpp
const uint64 imageId = FCyUIModule::Get().RegisterTexture(myTexture);
// In the draw callback (or ImageData for CyUI::Images / Viewport):
ImGui::Image(ImTextureRef(ImTextureID(imageId)), cyui::dp(256, 256));
// After the tool stops submitting this image:
FCyUIModule::Get().UnregisterTexture(imageId);
```

L’enregistrement retient une référence UObject forte. Le retrait attend les commandes Slate/rendu soumises avant libération : ne le faites pas à chaque peinture. Enregistrez/retirez hors callbacks, après arrêt des soumissions de l’ancienne image. Zéro et IDs périmés sont ignorés. Les IDs d’autres backends ImGui/RHI ne sont pas compatibles. Seules les UTexture ordinaires rendables par Slate sont prises en charge ; un pipeline GPU spécifique demande un adaptateur.

## Compatibilité et limites {#compatibility-and-limits}

Le connecteur vise UE 5.5+ Editor bureau Windows/Linux/macOS par API Slate publiques. L’installation validée localement est UE 5.8.4 Windows x64. Les autres versions et plateformes restent à vérifier sur leur installation.

Composition IME/candidats, fournisseurs d’accessibilité texte, tactile/manette et multi-viewports OS ne sont pas implémentés par ce backend de base. Caractères Unicode et presse-papiers texte sont pris en charge. Ne partagez pas ABI, headers, symboles ou textures avec un plugin Unreal ImGui tiers.

Web peut être sélectionné, mais WebCEF n’est pas empaqueté dans Unreal. Un backend Web Unreal doit respecter le runtime navigateur, sa version et le cycle de vie du moteur. Core fournit anglais/français ; sélectionnez la langue dans RunInContext ou le dessin du panneau. Les contextes gardent leurs langues ; menus/outils Slate utilisent la localisation Unreal. Voir [localisation](doc:localization).

## Licence {#license}

Connecteur et script de packaging CyUI restent MIT ; Dear ImGui et TextEditor gardent leurs notices MIT. Unreal reste externe sous [licence Epic](https://www.unrealengine.com/eula/unreal), sans redistribution ni changement de licence par CyUI. Préservez LICENSE et THIRD_PARTY_NOTICES.

Références : [création d’une fenêtre Slate](https://dev.epicgames.com/documentation/en-us/unreal-engine/slate-editor-window-quickstart-guide-for-unreal-engine), [FSlateVertex](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/SlateCore/FSlateVertex/Make), [FSlateRenderer](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/SlateCore/FSlateRenderer).
