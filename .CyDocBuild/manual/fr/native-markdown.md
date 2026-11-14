---
id: native-markdown
title: Markdown natif, édition et médias
description: Composer des documents, conversations et éditeurs natifs avec des adaptateurs optionnels.
section: Guides
order: 17
sourceRevision: 1
---

## Choisir les composants {#components}

Le lecteur et l’éditeur dessinent des widgets et textures Dear ImGui natifs.
Ils fonctionnent sans navigateur. Sélectionnez les cibles nécessaires :

| Cible | Usage | Dépendances |
| --- | --- | --- |
| `CyUI::DocumentViews` | Lecteur Markdown et conversations en flux | Code, Images, parseur MIT MD4C |
| `CyUI::DocumentEditor` | Modes Direct, Source, Côte à côte et Lecture | DocumentViews, CodeEditor |
| `CyUI::DocumentIO` | `.md`/`.cymd` en mémoire, conversion HTML du presse-papiers | DocumentViews, décompression MIT miniz |
| `CyUI::Media` | Contrôles natifs de lecture audio/vidéo | Images ; moteur de lecture de l’application |
| `CyUI::DocumentWeb` | Adaptateur optionnel des contenus web | DocumentViews, Web ; moteur de l’application |

```cmake
set(CYUI_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_GALLERY OFF CACHE BOOL "" FORCE)
set(CYUI_MODULES "DocumentEditor;DocumentIO" CACHE STRING "" FORCE)
add_subdirectory(external/CyUI)
target_link_libraries(app PRIVATE CyUI::DocumentEditor CyUI::DocumentIO)
```

Cette sélection ne compile ni Web, ni CEF, ni Media et ne charge aucun codec.
Recompilez les consommateurs après mise à jour : les modèles étendus modifient
l’ABI native. Les positions sont des octets UTF-8 aux limites des caractères.
Gardez un état distinct par vue.

## Lire et interagir {#reader}

```image
{"id":"native-reader","src":"assets/markdown-reader-fr.webp","alt":"Lecteur Markdown natif, image, tâches, tableau et conversation","controls":"minimal"}
```

`Document::setMarkdown` ne parse que le texte modifié. Le lecteur prend en charge
emphases imbriquées, gras, souligné (`<u>`), barré, `==surlignage==`/`<mark>`,
`<kbd>`, code en ligne/clôturé/indenté, citations, listes ordonnées/imbriquées,
tâches GFM, tableaux alignés avec barres échappées, liens de référence, ancres
de titres, URL HTTP(S) nues, entités nommées/numériques et `||spoilers||`.
Le HTML non reconnu reste du texte. Aucun moteur HTML/CSS ni script n’est exécuté.

```cpp
#include <cyui/modules/DocumentViews.h>
cyui::Document document;
cyui::DocumentState view;
document.setMarkdown("# Notes\n\n- [ ] Relire\n\n||Afficher||\n");
auto result = cyui::documentView("notes", view, document, {0, 420});
if (result.taskOffset != std::string::npos) {
    auto source = document.markdown();
    source[result.taskOffset] = result.taskChecked ? 'x' : ' ';
    document.setMarkdown(source);
}
// result.link et result.media sont des demandes pour les actions de l’application.
```

Sélectionnez le texte par glissement, puis copiez avec Ctrl+C. Un spoiler masqué
reste hors du texte sélectionnable jusqu’à sa révélation. Les liens internes
naviguent vers les ancres produites ; `DocumentState::jumpTo` permet aussi de
demander une ancre. L’état expose les positions de lecture. Une tâche retourne
son emplacement exact dans la source, sans modifier silencieusement le modèle.
Les conversations réutilisent ce rendu et mettent les messages en cache par ID
stable, y compris lors de l’arrivée progressive du Markdown.

## Images natives et contenus optionnels {#resources}

```image
{"id":"native-image-zoom","src":"assets/markdown-zoom-fr.webp","alt":"Image agrandie dans la vue native CyUI à l’intérieur de l’application","controls":"minimal"}
```

`DocumentServices::image(source)` retourne une `ImageData` en cache. PNG, JPEG,
SVG, GIF ou autre format est décodé par la chaîne d’image native de l’application ;
CyUI dessine elle-même la texture. Une animation peut retourner une trame différente.
Aucun élément HTML ni navigateur n’intervient. L’application possède les textures,
leur durée de vie et leur chargement ; le fournisseur s’exécute sur le thread UI
et doit rester non bloquant.

```cpp
cyui::DocumentServices services;
services.image = [&](const std::string& source) {
    return cachedNativeImages.resolve(source); // ImageData de l’application
};
cyui::DocumentOptions options;
options.services = &services;
auto result = cyui::documentView("notes", view, document, {0, 420}, options);
```

Les proportions restent conservées. `![légende|320x180](assets/photo.png)` fixe des
bornes logiques. Activez une image pour l’agrandir dans l’application : la vue
native permet zoom/déplacement, Échap ferme l’agrandissement. Le mode Direct éditable
ajoute une poignée de largeur. Un lecteur peut activer `resizeImages` et traiter
les propositions `resizedImage`/`resizedWidth` lui-même.

La syntaxe d’image visant une extension audio/vidéo connue produit une carte native.
`DocumentServices::embed` n’est appelé qu’après activation par l’utilisateur.
Sans moteur disponible, la carte de repli reste utilisable. `DocumentLinkPreview`
fournit depuis un cache titre, description, miniature et éventuellement URL/type
du contenu intégré. Une URL entre chevrons supprime la carte d’aperçu. Les
fournisseurs contrôlent accès aux ressources, réseau et stockage du cache ; le
lecteur ne télécharge rien de lui-même.

`mediaPlayer` reçoit un `MediaBackend` : ses instantanés fournissent trames et état,
et les contrôles natifs demandent lecture/pause, déplacement, sourdine et volume.
Le moteur peut déjà servir ailleurs dans l’application. Aucun fichier n’est décodé
et aucun périphérique audio n’est ouvert par le widget. Le déplacement se valide
au relâchement.

Pour le web, reliez `services.embed` à `DocumentWebAdapter::draw`. Son callback
`backend` retourne un `WebBackend` existant pour l’ID stable du média : une vue
CEF CyUI ou l’adaptateur de l’application. Une modification d’URL/moteur demande
une navigation ; les événements remontent à l’application. Celle-ci possède
politique, durée de vie, initialisation et pompage des frames, y compris les
panneaux masqués ; suivez le [cycle Web](doc:web#native-lifecycle). Aucun second
moteur n’est créé.

## Éditer un document {#editor}

```image
{"id":"native-live-editor","src":"assets/markdown-live-fr.webp","alt":"Éditeur Direct natif avec texte enrichi, tâches éditables et image native","controls":"minimal"}
```

```image
{"id":"native-split-editor","src":"assets/markdown-split-fr.webp","alt":"Source Markdown à côté du document rendu nativement","controls":"minimal"}
```

```cpp
#include <cyui/modules/DocumentEditor.h>
cyui::DocumentEditor editor;
editor.setMarkdown("# Notes\n\nDu **texte formaté**.\n");
cyui::DocumentEditorOptions options;
options.document.services = &services;
if (editor.draw("editor", {0, 560}, options).changed)
    markDocumentDirty(editor.markdown());
```

Direct utilise des champs natifs avec les marqueurs masqués et des glyphes formatés
aux positions de sélection et de retour à la ligne calculées par le champ. Images
et tableaux sont natifs. Les blocs de code sont éditables avec coloration ; la
structure des tableaux se modifie dans Source. Source utilise CodeEditor ; Côte à
côte ajoute un aperçu synchronisé par positions source. Lecture emploie le même
lecteur. Changer de mode préserve le document. Les modes natifs partagent
l’historique de transactions de l’éditeur source.

Les barres principale/flottante et les commandes couvrent gras, italique, souligné,
barré, spoiler, code en ligne, liens, titres, listes, tâches, citations et indentation.
`select(first,last)`, `insert(markdown)` et `command(...)` servent aux menus de
l’application. L’éditeur signale les changements sans enregistrer de fichiers.
Ctrl/Cmd+1–4 change de mode ; les raccourcis courants et rechercher/remplacer sont
disponibles. Direct normalise la représentation Markdown du bloc modifié ; les
autres blocs restent intacts. Sa sélection de texte appartient au bloc : Source
permet les sélections/modifications entre plusieurs blocs. Les blocs clôturés
vides, les éléments de liste vides et le code imbriqué dans des citations ou listes
s’ouvrent dans Source pour préserver leur structure.

`pasteRich` importe HTML/images du presse-papiers via les services de l’application
et retourne du Markdown référençant les ressources enregistrées. Ctrl+Maj+V conserve
le collage simple. Coller une URL HTTP(S) sur une sélection crée un lien. `drop`
accepte le type de charge ImGui choisi et retourne le Markdown importé ; l’hôte
convertit les dépôts de fichiers OS vers cette charge ou appelle `insert`.
Aucun backend de presse-papiers OS ni gestionnaire de fichiers n’est chargé.

`spelling(markdown, language)` est un fournisseur optionnel mis en cache : il
retourne des plages source UTF-8 et des suggestions. Direct souligne ces plages ;
le menu de suggestions applique les corrections dans le même historique.
Modifiez `spellingRevision` lors d’un changement de résultats/dictionnaire. Aucun
dictionnaire n’est embarqué ou choisi silencieusement. La lecture seule désactive
les modifications et suggestions.

## Fichiers, presse-papiers et export HTML {#io}

`decodeMarkdown`/`encodeMarkdown` préservent UTF-8/UTF-16 LE/BE, BOM et fins de ligne.
`readCyMD`/`writeCyMD` gèrent l’archive `document.md`, `assets/`, `previews.json`.
L’import ZIP accepte stockage et DEFLATE ; l’export utilise le stockage simple.
Les interfaces consomment/retournent des tampons mémoire et n’extraient aucun
fichier sur disque. Gardez ressources et cache d’aperçus avec le document.

Limites par défaut : archive 64 Mio, contenu décompressé 128 Mio, entrée 32 Mio,
4096 entrées. Traversées de chemin, doublons, entrées chiffrées/non reconnues,
liens symboliques, erreurs de somme de contrôle et dépassements sont refusés.
ZIP64 n’est pas pris en charge. `documentAssetPath` aide à valider les clés ;
appliquez aussi la politique de fichiers de l’application.

`markdownFromHtml` convertit formats courants, liens, paragraphes, listes, tableaux simples et code
du presse-papiers. Scripts/styles sont écartés. L’importateur d’images explicite
doit enregistrer les ressources. Cet utilitaire n’est pas un moteur HTML/CSS.
`documentHtml` exporte du HTML portable échappé, sans script ni téléchargement ;
les références de médias relatives doivent être distribuées avec l’export.
`documentPackageHtml(package, title)` embarque les ressources référencées sous forme
d’URL data pour produire un fichier autonome. `manifest` conserve les métadonnées de l’archive.

## Python et vérifications {#python}

Python propose `documents`, `documenteditor`, `documentio`, `documentweb` et
`media`. Le parsing utilise les dépendances MIT Python pures `markdown-it-py` et
`linkify-it-py`. Les imports de modèles ne chargent ni imgui-bundle, ni CEF, ni
moteur de lecture. Services de ressources et backends suivent la même architecture
que le C++. Les positions publiques des modèles document/éditeur sont des octets
UTF-8. Les callbacks avancés de TextEditor et les capacités de son éditeur source
restent des API C++. L’éditeur Python propose l’édition basique par blocs, les
quatre vues et l’historique commun ; barres flottantes, correction orthographique,
redimensionnement d’images, rechercher/remplacer source, défilement synchronisé et
sélection par glissement du lecteur sont actuellement des fonctions C++.

La galerie présente lecteur/conversation à l’onglet 18, éditeur à l’onglet 24 et
moteur de lecture simulé explicitement identifié à l’onglet 25. Les builds Windows
et tests d’interaction sans fenêtre couvrent l’implémentation native. Exécution
Linux/macOS, intégration IME de plateforme et moteurs externes demandent une
validation dans l’application cible ; la portabilité des sources seule ne
démontre pas ces résultats.
