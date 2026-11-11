---
id: paged-code
title: Éditeur de code paginé
description: Édition virtuelle réutilisable avec stockage, révisions et transactions gérés par l’hôte.
section: Guides
parent: modules
order: 31
sourceRevision: 1
---

`CyUI::Code` fournit `CodePaging.h` : positions UTF-16, fragments UTF-8 bornés,
validation et construction d’une ligne en flux. `CyUI::CodeEditor` fournit
`PagedCodeEditor.h`, également inclus par `CodeEditor.h`. Il affiche et édite un
document virtuel sans transmettre une page au backend TextEditor prévu pour un
document complet. Il ne dépend ni de CyCodeCore, ni du système de fichiers, de
MCP ou d’un serveur de langage.

## Sélectionner le composant {#integration}

```cmake
set(CYUI_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_GALLERY OFF CACHE BOOL "" FORCE)
set(CYUI_MODULES "CodeEditor" CACHE STRING "" FORCE)
set(CYUI_IMGUI_TARGET app_imgui)
add_subdirectory(external/CyUI)
target_link_libraries(app PRIVATE CyUI::CodeEditor)
```

Utiliser C++20 et une seule instance compatible d’ImGui. Code seul reste en
C++17. Aucune dépendance d’exécution supplémentaire n’est ajoutée. Le packaging
des sources Unreal inclut les nouvelles sources lorsque CodeEditor est choisi.
Cette évolution fournit des API C++ natives ; elle n’ajoute pas de bindings
d’éditeur virtuel pour Python/Web.

## Contrat de l’hôte et de la vue {#contract}

Conserver un `PagedCodeEditor` par vue, utilisé sur son thread UI/ImGui. Fournir
un `PagedCodeDocument` avec identifiant de document stable non nul, révision de
contenu non nulle, nombre de lignes logiques et état lecture seule. Une source
vide possède une ligne logique ; un saut de ligne final ajoute une ligne vide.
Les révisions sont des gardes d’égalité, pas des dates. Un changement de
document/révision annule les lectures et interactions devenues obsolètes.

Les IDs de requête et d’édition sont propres à chaque vue. Renvoyez les réponses
asynchrones et validations à la vue d’origine ; ajoutez une clé de vue applicative
lorsque plusieurs éditeurs du même document partagent une file de requêtes.

`draw` renvoie une fois un `PagedCodeRead` lorsque la zone visible exige des
données. La requête contient son ID, les gardes document/révision, un intervalle
de lignes, une fenêtre de cellules d’affichage, la largeur des tabulations et un
budget d’octets. La préparer hors du dessin, éventuellement dans un worker
appartenant à l’application. Appeler `supplyPage` sur le thread UI. Les réponses
d’anciens ID de requête, documents ou révisions sont refusées. `cancelledRead`
permet à l’hôte d’annuler sa tâche remplacée. Le widget ne démarre aucun worker
ni aucune entrée/sortie.

Renvoyer un `PagedCodeLine` par ligne logique demandée, avec les métriques
UTF-16/cellules de la ligne complète et uniquement son fragment visible borné.
Le validateur contrôle identité, couverture, limites UTF-8, tokens et budgets.
Les données manquantes/échouées restent signalées comme en chargement/indisponibles,
avec une action Réessayer. La vue utilise des origines logiques 64 bits et des
barres normalisées plutôt qu’un immense canevas ImGui en virgule flottante ;
la navigation précise par ligne reste entière.

```cpp
cyui::PagedCodeEditor view; // conserver avec ce panneau
view.setDocument({documentId, contentRevision, logicalLines, 0, readOnly});
// À chaque frame :
auto result = view.draw("##source", cyui::dp(0, 400));
if (result.read) host.queueVisiblePage(*result.read);
if (result.edit.committed()) host.applyGuardedReplacement(result.edit.value);
for (const auto& action : result.actions) host.handleCodeAction(action);
// Livrer les lectures terminées à view.supplyPage(...) sur le thread UI.
```

Ces noms d’hôte illustrent les responsabilités, pas une autre bibliothèque
obligatoire. Utiliser `pendingRead`, `failRead` et `retryRead` dans l’adaptateur
asynchrone.

## Coordonnées et lignes longues {#coordinates}

Les positions utilisent des lignes logiques et unités UTF-16 commençant à zéro.
Elles se distinguent des offsets d’octets UTF-8, cellules et tabulations. Les
fragments excluent les sauts de ligne et le BOM initial ; l’hôte conserve tous
les octets originaux. Les tokens utilisent des offsets UTF-8 relatifs au
fragment et les rôles sémantiques de la palette.

`PagedCodeLineBuilder` accepte des coupures UTF-8 arbitraires entre les blocs
d’une même ligne logique. Il ne conserve que le fragment horizontal demandé et
calcule les métriques UTF-16/cellules complètes. Il peut résoudre une position
UTF-16 via `revealedColumn()` / `revealedByte()`, en refusant une coupure de paire
surrogate. Une position au-delà d’une ligne raccourcie est ramenée à sa fin.
Pour une lecture `reveal`, conserver la fenêtre demandée si elle montre déjà
cette position ; sinon renvoyer une fenêtre la contenant et adapter
`page.firstColumn`.

L’affichage compte une cellule par scalaire BMP, deux par scalaire supplémentaire
et les arrêts de tabulation configurés. La tabulation vaut quatre par défaut.
La mise en forme des écritures complexes/larges/combinées, le bidi, le déplacement
par graphèmes et le retour visuel à la ligne ne sont pas implémentés. La police
monospace, la palette, le zoom Ctrl+molette par vue et le DPI/style de l’hôte sont
partagés avec Code.

Le budget concerne chaque page, pas le fichier entier : au plus 4 Mio, 4 096
lignes demandées, 8 192 cellules horizontales, 64 Kio par fragment de ligne et
65 536 tokens. La file d’entrée est bornée à 256 événements et au budget
d’insertion configuré (1 Mio par défaut, au plus 4 Mio). Les dépassements sont
explicites ; un fragment incomplet ne devient jamais un document complet.

## Modifications et propriétaire de l’Undo {#transactions}

Saisie, collage, Entrée/Tab et suppression produisent des propositions atomiques
`EditPhase::commit` contenant ID stable, révision attendue, sélection UTF-16
ordonnée et remplacement UTF-8. Le widget ne modifie pas le modèle applicatif et
n’enregistre aucun fichier. Valider et appliquer une transaction Undo native,
puis appeler `acceptEdit(id, newSnapshot, resultingSelection)`. La révision
acquittée doit différer. Signaler un refus par `rejectEdit`.

La saisie est mise en file derrière une modification asynchrone ; elle n’est pas
jetée à chaque frame d’attente. Un refus ou une révision externe annule ces
intentions et les renvoie dans `cancelledInput` pour récupération par l’hôte,
avec la proposition annulée. Ne pas les rejouer silencieusement sur un autre
document. Terminer ou résoudre explicitement les entrées en attente avant de
détruire une vue. Après un changement externe, l’hôte peut restaurer une
sélection recalculée par `setSelection`.

La sélection à la souris expose begin/update/commit/cancel, avec annulation par
Échap. Flèches, Début/Fin, Page précédente/suivante et Ctrl/Cmd+A/Début/Fin sont
pris en charge. Copier/Couper, Undo/Redo, Enregistrer et Rechercher/suivant/précédent
sont des actions protégées de l’hôte : celui-ci lit les sélections entre pages,
effectue la recherche globale et possède le presse-papiers/l’historique. Copier
la sélection exacte protégée avant d’appliquer Couper. Aucun second historique
de texte ni recherche faussement globale limitée à la vue n’est créé.
Utiliser `keyboardShortcuts=false` et `requestAction` pour les profils applicatifs.

Pour CyCodeCore, l’adaptateur applicatif convertit ces plages UTF-16 vers les
opérations protégées octets/lignes et le propriétaire natif Apply/Undo. Il fournit
les métriques de pages/lignes et le véritable contexte de syntaxe. Le Core reste
indépendant de CyUI. Cette modification de bibliothèque n’installe aucun
adaptateur Core/IDE/Reader ni overlay LSP paginé.

## Syntaxe et exemple {#syntax}

Fournir les rôles de tokens préparés avec le contexte lexical/sémantique réel.
Un fragment isolé ne permet pas de connaître un commentaire multiligne ou
langage imbriqué commencé avant lui. La limite de snapshot et l’API existantes
de `highlightCode` restent inchangées ; la vue ne charge pas et ne recolorise pas
le fichier entier en cachette. Curseurs multiples, pliage, menus de complétion et
déplacement par mots ne font pas partie de cette première vue paginée.

La page Code de Gallery bascule entre l’éditeur existant en mémoire et **Éditeur
paginé**. Son petit hôte éditable montre transactions, sélection, presse-papiers
et Undo/Redo de l’hôte. Une source séparée en lecture seule d’un million de lignes
ne génère que les lignes demandées, dont une ligne longue traitée en flux. Ces
données de démonstration sont synthétiques ; elles ne constituent pas une mesure
sur un fichier disque d’un million de lignes.

```text
build/cyui_demo.exe --gallery-only --tab 10 --paged-code --capture paged.bmp --theme navy
```

Le modèle éditable de l’exemple est limité à 128 Kio : c’est une politique de
stockage de démonstration, pas une limite de document du widget paginé.
