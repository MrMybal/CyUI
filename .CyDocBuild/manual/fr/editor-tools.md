---
id: editor-tools
title: Outils d’édition
description: Inspecteur typé, hiérarchies, tableaux, sélection de fichiers et commandes partagées.
section: Guides
order: 11
sourceRevision: 2
---

Les modules d’édition réutilisables reposent sur Core. Sélectionnez `Inspector`,
`Hierarchy`, `DataView`, `FilePicker`, `Commands`, `Nodes` et `Timeline` selon vos
besoins ; l’application conserve modèles, accès aux ressources et transactions Undo.

## Inspecteur de propriétés {#inspector}

Inspector accepte booléens, int64 exacts, doubles, floats, vecteurs 2/3/4, couleurs
RGBA, références de ressources typées, tableaux homogènes récursifs, structures
typées et valeurs JSON personnalisées. Donnez à chaque propriété un ID stable non nul et une
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

DataView ajoute tri numérique multicolonne, filtres, colonnes redimensionnables,
réorganisables et masquables, et lignes limitées à la zone visible. Les cellules
sont texte, int64, double ou booléen. Double-clic pour éditer, Entrée pour valider,
Échap pour annuler. Maj-clic sélectionne un rectangle ; Ctrl+C/V échange du TSV
avec guillemets, tabulations et sauts de ligne conservés. Les doubles acceptent
expressions et bornes. Le collage valide tout le lot, y compris lecture seule et
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

Hierarchy, DataView et FilePicker dépendent seulement de Core et de C++17. Core
compile désormais `src/Numeric.cpp`. Les builds manuels ajoutent aussi les unités
InspectorData, CommandRegistry, NodeData et TimelineData aux modules concernés.
CMake et le script de packaging Unreal sélectionnent ces fichiers automatiquement.

Python fournit les modules correspondants, modèles de presse-papiers typés,
registre de commandes et fonctions de graphes/timelines. Ils s’importent sans
imgui-bundle ; le dessin le charge à la demande. PropertyFloat distingue float32
de double. Les enveloppes JSON natives et Python sont interchangeables.

Onglets de la galerie : Inspector **12**, Nodes **7**, Timeline **8**, Commands
**16**, Hierarchy **30**, Data view **31**, File picker **32**. Vérifiez les cinq
thèmes et les deux densités avec les données de votre application.
