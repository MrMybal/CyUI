---
id: editor-tools
title: Outils d’édition
description: Inspecteur typé, hiérarchies, tableaux, sélection de fichiers et commandes partagées.
section: Guides
order: 11
sourceRevision: 3
---

Les modules d’édition réutilisables reposent sur Core. Sélectionnez `Inspector`,
`Hierarchy`, `DataView`, `FilePicker`, `Commands`, `Nodes` et `Timeline` selon vos
besoins ; l’application conserve modèles, accès aux ressources et transactions Undo.

## Inspecteur de propriétés {#inspector}

Inspector accepte booléens, int64 exacts, doubles, floats, vecteurs 2/3/4, couleurs
RGBA, références de ressources typées, tableaux homogènes récursifs, structures
typées, Sets, Maps, valeurs optionnelles, enums/flags et JSON personnalisé.
Donnez à chaque propriété un ID stable non nul et une
`key` non traduite. `category` ajoute un en-tête ; `defaultValue` permet la remise
à zéro. Lecture seule, bornes, choix et validation hôte encadrent les modifications.

Un clic droit sur un champ copie/colle son JSON ; sur une catégorie, il copie/colle
tous ses champs. Les enveloppes utilisent `format:"cyui.inspector"`, `version:1`,
`kind:"property"` et une `property` typée, ou `kind:"category"` et une table
`properties`. Les catégories correspondent par clés et types, indépendamment des
IDs d’objets, traductions ou noms de catégories. Dimensions des vecteurs et classes
de ressources doivent correspondre. Le collage valide tous les champs avant de
retourner les commits ; l’hôte les applique dans une transaction Undo unique.
Un champ en lecture seule ou invalide refuse toute la catégorie. Le JSON est
limité à 1 Mio, 32 niveaux JSON et 65536 nœuds, à l’import comme à l’export.
Ces budgets de presse-papiers sont indépendants de la taille affichée.

```cpp
cyui::Property strength;
strength.id = 1;
strength.key = "strength";
strength.label = "Strength";
strength.category = "Material";
strength.value = 2.56;
strength.bounded = true;
strength.minimum = 0;
strength.maximum = 10;
auto json = cyui::propertyToJson(strength);
auto paste = cyui::propertyFromJson(strength, json);
// Validez paste ; appliquez tous ses edits dans une transaction hôte.
```

`commonProperties` combine les champs des objets sélectionnés, signale les valeurs
différentes, intersecte les bornes et respecte la lecture seule. Appliquez un commit
à tous les objets correspondants ensemble. Gardez un InspectorState par vue et
incrémentez sa révision lors d’un remplacement externe. L’onglet Inspector de la
galerie présente un matériau et les propriétés avancées avec édition de deux objets.

### Tableaux virtuels et types personnalisés {#virtual-arrays}

Les lignes réutilisent tous les éditeurs : booléens, int64 exacts, expressions
float/double, textes/choix, vecteurs, couleurs, ressources, tableaux, structures et
valeurs personnalisées. Seules les lignes visibles et la ligne active hors écran
sont dessinées. La limite d’affichage de 1024 éléments est supprimée (indices jusqu’à
INT_MAX). Les tableaux imbriqués et structures ouvrent un éditeur de détail, ce qui
garde une hauteur de ligne constante. Le menu contextuel copie/colle le JSON typé,
duplique, déplace ou supprime une ligne. L’onglet **Tableaux virtuels** de la galerie
propose jusqu’à 100000 éléments.

```cpp
cyui::Property points;
points.id = 10;
points.label = "Points";
points.value = cyui::makePropertyArray(cyui::PropertyVector{{0,0,0}}, 10000);
cyui::PropertyObject spawn{"SpawnPoint", {
    {"Position", cyui::PropertyVector{{1,2,3}}},
    {"Weights", cyui::makePropertyArray(.5f, 3)}
}};
points.value = cyui::makePropertyArray(spawn, 10000);
```

`makePropertyArray(elementTemplate,count=0)` conserve le type même à vide et fournit
la valeur par défaut du bouton Ajouter. `PropertyArray{{values...}}` déduit son type
du premier élément ; un tableau vide sans modèle ajoute un texte. Les structures
utilisent des clés et un nom de type stables. L’imbrication est limitée à 12 niveaux
de conteneurs. Les bornes/choix du parent s’appliquent récursivement ; la validation
hôte reçoit toute la modification.

`PropertyCustom{type,json}` adapte les types de l’application avec du JSON strict.
`InspectorOptions::editCustom(id,path,value)` retourne un `Edit<PropertyCustom>`
avec les phases begin/update/commit/cancel. Dessinez une ligne de hauteur de frame
en respectant la largeur courante. Sans callback, un éditeur JSON est disponible.
`browseAssetAt(id,path)` identifie les ressources imbriquées. Les chemins contiennent
les indices et clés de champs. Modèles, Undo et chargement des ressources restent
dans l’application. Il s’agit d’un adaptateur explicite, pas de réflexion C++ automatique.

`arrayHeight` vaut 180 unités logiques par défaut. `InspectorState::lists` expose
`renderedRows` pour la dernière frame. Les grands brouillons sont en cache : augmentez
la révision à chaque modification/remplacement externe du modèle, même hors édition.
La copie et la validation complètes se produisent lors des modifications, pas à
chaque frame au repos.

Python propose `PropertyObject`, `PropertyCustom`, `make_property_array` et
`inspector(..., array_height=180, edit_custom=..., browse_asset=...)`. Les éléments
numériques acceptent directement les expressions avec Entrée. Le callback retourne
un `Edit` et reçoit le même ID racine/chemin. Le JSON reste en version 1 avec les
modèles `element` optionnels des tableaux, `object`/`objectType` et `custom`/`customType`.

### Sets, Maps et édition groupée {#collections}

`makePropertySet(elementTemplate)` et `makePropertyMap(keyTemplate,valueTemplate)`
conservent leur schéma à vide. Un Set stocke des éléments typés uniques dans
`values` ; une Map stocke des entrées `{key,value}` dans `entries`. Tous les types
de propriétés fonctionnent comme élément, clé ou valeur, y compris structures
récursives et types personnalisés. L’ordre ne change pas leur identité ; la vue
garde l’ordre d’insertion. L’unicité est vérifiée après normalisation/clamp : un
doublon refuse toute la modification ou le collage. Ajouter ouvre un brouillon
pour saisir une clé/un élément unique avant insertion.

```cpp
auto tags = cyui::makePropertySet(std::string("Tag"));
tags.values = {std::string("Player"), std::string("Visible")};
auto weights = cyui::makePropertyMap(std::string("Key"), .5f);
weights.entries = {{std::string("Walk"), .7f}};
auto optional = cyui::makePropertyOptional(std::int64_t(0)); // absent
cyui::PropertyEnum quality{"Quality", 1, {{0,"Low"},{1,"Medium"},{2,"High"}}};
cyui::PropertyEnum flags{"Channels", 3, {{1,"R"},{2,"G"},{4,"B"}}, true};
```

Array, Set et Map partagent lignes virtuelles, recherche et sélection Ctrl/Maj.
Ctrl+A sélectionne les lignes filtrées ; Suppr retire la sélection. **Modifier la
sélection** applique une expression ou du JSON typé atomiquement ; `Value` désigne
la valeur initiale de chaque élément. Une Map modifie ses valeurs. Les tableaux
permettent aussi duplication et réorganisation de la sélection en glissant les
indices. `editPropertyCollection` expose ces opérations sans dessin. Les pages
Sets/Maps de la galerie proposent 100000 lignes.

`PropertyObject::metadata[fieldKey]` fournit les règles `PropertyMetadata` de chaque
champ : libellé/aide, lecture seule, bornes dont int64 exact, choix, mot de passe
et texte multiligne. Ces métadonnées appartiennent à la destination et ne sont
jamais remplacées par le JSON collé. Les opérations structurelles sur tableaux
conservent les champs de structures en lecture seule. Une valeur optionnelle garde
son modèle immuable et une valeur nullable ; type/choix des enums et bits autorisés
des flags sont vérifiés. `propertyEditor` intègre tout éditeur dans une vue hôte
avec les mêmes validations et phases transactionnelles.

Le JSON version 1 ajoute `set` (éléments typés et modèle `element` optionnel),
`map` (`value:[{"key":typed,"value":typed}]`, `keyTemplate`, `valueTemplate`),
`optional` (`value` null ou typée et `element`), `enum`/`flags` (`enumType`, `value`
int64 et `items` de forme `{value,label}`). Les budgets restent identiques.
Python propose les mêmes types/factories et `edit_property_collection` ; les entrées
Map sont des couples `(key,value)`, les bornes des métadonnées utilisent
`minimum`/`maximum` ou `None`. L’éditeur autonome est `property_value_editor`.

## Expressions numériques {#expressions}

Double-cliquez sur un nombre, donnez-lui le focus puis Entrée, ou choisissez
**Modifier l’expression...** dans son menu. `Value` désigne la valeur au début de
l’édition. Entrée évalue puis borne le résultat ; une expression invalide laisse
le champ ouvert avec une erreur. Exemple : **2,56 → Value*100 → 10** si le maximum
vaut 10.

La saisie numérique de Core accepte aussi ces expressions dans les champs à
glisser, curseurs, contrôles audio et composantes vectorielles de l’inspecteur.
Syntaxe : `+ - * / % ^`, parenthèses, `Value`, `pi`, `e`, suffixe `k`, et fonctions
`abs`, `sqrt`, `min`, `max`, `clamp`, `round`, `floor`, `ceil`. Utilisez le point
décimal et des virgules entre arguments. Limites : 4096 octets et 32 opérations
imbriquées. Divisions par zéro, fonctions inconnues, résultats non finis et code
exécutable sont refusés.

Le presse-papiers int64 et les littéraux entiers conservent tous les bits. Quand
long double possède la précision de double, les expressions partant au-delà de
±(2^53−1) sont refusées ; utilisez alors un littéral entier exact.

## Hiérarchies et tableaux de données {#views}

Hierarchy consomme des éléments parent/enfant plats, avec parent 0 pour la racine.
La recherche conserve les ancêtres ; Ctrl/Maj sélectionnent. Renommage, duplication,
suppression, visibilité et verrouillage produisent des requêtes. Déposez sur une
ligne pour changer de parent, avec Maj pour insérer avant, ou dans la zone racine.
Les déplacements cycliques, verrouillés ou refusés par l’hôte sont rejetés.
L’application applique elle-même les changements à son modèle.

Les flèches parcourent les lignes visibles : Droite ouvre/entre dans les enfants,
Gauche ferme/sélectionne le parent. Début/Fin/Page préc./Page suiv., Ctrl/Maj,
Espace et Ctrl+A fonctionnent avec le filtre. F2/Entrée demande le renommage ;
Suppr demande la suppression. `navigateHierarchy` fonctionne sans rendu.

DataView ajoute tri numérique multicolonne, filtres, colonnes redimensionnables,
réorganisables et masquables, et lignes limitées à la zone visible. `DataCell` est
`PropertyValue` : tous les types Inspector fonctionnent, dont vecteurs/couleurs/
ressources, collections, optionnels, enums/flags, structures et types personnalisés.
Le double-clic ouvre l’éditeur correspondant. `DataColumn::metadata` fournit les
règles ; `DataViewOptions::editors` fournit les callbacks de ressources/types
personnalisés (ID de colonne, chemin commençant par l’ID de ligne). Maj-clic
sélectionne un rectangle ; Ctrl+C/V échange du TSV avec guillemets, tabulations et
sauts de ligne conservés. Les cellules riches contiennent du JSON typé ; les
primitives restent en texte simple. Les nombres acceptent expressions et bornes.
Le collage valide tout le lot, y compris lecture seule et
validation applicative. Une nouvelle révision annule l’édition active.

## Fichiers et commandes {#files-and-commands}

FilePicker affiche une liste de dossier en cache fournie par l’hôte pour ouvrir
un ou plusieurs fichiers, enregistrer ou sélectionner un dossier. La navigation
demande un nouveau dossier ; l’application fournit les résultats asynchrones du
système de fichiers réel ou virtuel. Une liste en chargement, périmée ou en erreur
ne peut être validée. Les filtres d’extension ignorent la casse, les chemins restent
lexicalement sous la racine et un remplacement demande confirmation. L’application
gère permissions, liens symboliques, vérifications finales et opération réelle.

CommandRegistry partage les métadonnées entre menus, barres d’outils, palettes et
raccourcis. Le premier contexte actif précède les commandes globales. La saisie de
texte bloque les raccourcis sauf autorisation explicite. Les conflits dans un même
contexte sont refusés ; des contextes différents peuvent partager un raccourci.
shortcutEditor permet de le changer ; l’enveloppe JSON `cyui.commands` sauvegarde
les associations. Le chargement est atomique. Appelez poll une fois par frame dans
la fenêtre hôte focalisée et exécutez l’ID retourné dans l’application.

## Édition de graphes et timelines {#graphs-and-timelines}

Nodes accepte groupes de commentaires et points de reroutage. Les groupes se
déplacent, se redimensionnent et se renomment ; les points se déplacent. Le menu
contextuel crée/supprime ces éléments. Ctrl+C/V/D échange les nœuds sélectionnés
avec leurs liens internes et points de reroutage. Le JSON utilise `cyui.nodes`,
version 1. Le clonage alloue de nouveaux IDs et remappe les références ; les liens
extérieurs sont exclus. L’hôte applique créations, commits et suppressions.

Timeline découpe à la tête de lecture (**S**), duplique (**Ctrl+D**), édite les
marqueurs et réordonne les pistes en glissant leurs noms. L’application fournit
un allocateur d’IDs ; les clips verrouillés et durées minimales sont respectés.
Marqueurs et ordre des pistes retournent des vecteurs d’édition distincts. Échap,
lecture seule et révision différente annulent les gestes. Moteurs et lecture
appartiennent toujours à l’application.

## Intégrations et exemples {#integrations}

Hierarchy et FilePicker dépendent de Core ; DataView dépend aussi d’Inspector
pour réutiliser ses éditeurs. Tous utilisent C++17. Core
compile désormais `src/Numeric.cpp`. Les builds manuels ajoutent aussi les unités
InspectorData, CommandRegistry, NodeData et TimelineData aux modules concernés.
CMake et le script de packaging Unreal sélectionnent ces fichiers automatiquement.

Python fournit les modules correspondants, modèles de presse-papiers typés,
registre de commandes et fonctions de graphes/timelines. Ils s’importent sans
imgui-bundle ; le dessin le charge à la demande. PropertyFloat distingue float32
de double. Les enveloppes JSON natives et Python sont interchangeables.

Onglets de la galerie : Inspector **12**, Nodes **7**, Timeline **8**, Commands
**16**, Hierarchy **30**, Data view **31**, File picker **32**, Tableaux virtuels
**33**, Sets **34**, Maps **35**, Tableau riche **36**, Espace d’édition **37**. Vérifiez les cinq
thèmes et les deux densités avec les données de votre application.

Le déplacement d’un groupe Nodes déplace aussi les nœuds dont l’ancrage était dans
son rectangle initial et leurs points de routage internes. Appliquez les commits
`groups`, `moved` et `reroutes` ensemble. Échap/révision annule tous les aperçus.
`moveGroupContents=false` désactive ce comportement ; le redimensionnement garde
le contenu fixe. Python possède le même comportement. Timeline propose aussi
**Ajouter un marqueur** en Python et le helper autonome `add_timeline_marker`.

Espace d’édition relie Hierarchy, Inspector, DataView et la sélection de ressources
à un modèle hôte et une sélection partagés, avec Undo/Redo par Ctrl+Z/Ctrl+Y.
`include/cyui/examples/EditorWorkspace.h` valide un lot complet avant de modifier
modèle/historique. Adaptez ses snapshots à l’historique de votre application ;
l’accès aux ressources reste géré par l’hôte.
