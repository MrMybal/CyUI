---
id: code
title: Syntaxe C++ et extensions
description: Utiliser les 102 modes natifs et enregistrer une syntaxe applicative.
section: Guides
parent: modules
order: 30
sourceRevision: 4
---

Liez `CyUI::Code` (C++17) pour le catalogue, les lexers et les vues en lecture seule ; `CyUI::CodeEditor` (C++20) ajoute l’édition. Core seul ne compile ni ne lie de lexer. CMake résout les dépendances et exporte `CYUI_CODE_LANGUAGE_CATALOGUE=1`. Le catalogue commité se compile sans réseau ni Python, avec une seule instance compatible d’ImGui.

## Couleurs de syntaxe communes {#syntax-colors}

La palette de code par défaut reprend les captures Rider fournies : texte clair,
mots-clés/directives bleus, types mauves, nombres vert pâle, chaînes orange,
commentaires verts et fonctions turquoise. Les lexers de tous les langages
emploient ces mêmes rôles, y compris les commentaires de documentation et TODO.
Accents d'interface, avertissements et statuts restent indépendants de la syntaxe.

Core expose codeText, codeKeyword, codeType, codeNumber, codeString, codeComment,
codePreprocessor et codeFunction dans Palette, l'éditeur et les fichiers de thème.
Code/CodeEditor/Diff natifs, les adaptateurs lexicaux/parseurs de Reader et les
vues Python les partagent. Le contraste s'adapte au thème ; sur fond clair les
teintes deviennent plus sombres tout en restant orange/vertes. Les anciens thèmes
héritent des champs manquants de leur palette de base. Recompilez les consommateurs
natifs avec le nouvel en-tête Palette. Reader protège commentaires et chaînes
lexicaux des surcharges sémantiques, y compris celles des régions inactives.

## Police de code, zoom et guides de blocs {#text-presentation}

`loadFonts()` charge JetBrains Mono Regular 2.304 comme `monoFont()` depuis les
octets originaux embarqués. Code, CodeEditor, Diff et le code Markdown natif
emploient cette police commune, sans installation, chemin externe ni réseau.
Le paquet Python contient la même police Regular. Conservez son copyright et
`third_party/jetbrains-mono/OFL.txt` dans les distributions. La police reste sous
SIL OFL 1.1 et le code propre à CyUI sous MIT. Le rendu ne forme pas de ligatures.

Dans `codeViewer`, `diffViewer` et `CodeEditor` natifs, Ctrl + molette verticale
zoome uniquement le canevas de code survolé, de 50 % à 300 %. La molette simple
défile toujours. Les menus et autres vues gardent leur taille ; les deux colonnes
de Diff partagent la même échelle. Le zoom ne change ni texte, révision, sélection
ni Undo. Les DPI et la taille globale de l'interface restent indépendants.
Les traits de portée concernent les blocs entre accolades ; les arguments entre
parenthèses gardent le fond des paires, sans trait au travers des identifiants.

Réglez `CodeViewState::textScale` / `DiffState::textScale`, ou utilisez
`CodeEditor::setTextScale(float)` et `textScale()` ; 1 réinitialise la vue.
`wheelZoom=false` ou `setWheelZoomEnabled(false)` désactive la molette sans retirer
le réglage programmatique. Les valeurs sont bornées à 0,5..3 ; NaN revient à 1.
Ces API natives sont additives : recompilez les consommateurs avec les en-têtes.
Python partage la police ; ces nouvelles API de molette/état ne sont pas des
bindings Python. Les aperçus de code en lecture seule partagent aussi la police.

Les contrôles de sources/données, compilations de production et régressions de
syntaxe sont distincts de la validation native/graphique. Molette/pavé tactile,
multimoniteur, pixels thème/DPI, rasterisation et Linux/macOS restent à valider
pendant la suspension des exécutions natives sur ce poste.

## Modes intégrés {#built-in-modes}

`codeLanguages()` retourne 102 instantanés de métadonnées possédées. `data/code-languages.json` définit la liste canonique, les associations et les profils de shaders. Le catalogue couvre les modes de syntaxe de Notepad++ utilisés pour l’intégration, plus HLSL, GLSL, Markdown, TSX et un profil Unreal C++. Chaque mode possède un exemple. NFO s’affiche volontairement en texte brut : le décodage OEM relève de l’hôte. Internal Search colore une liste de diagnostics, sans moteur de recherche. Aucun composant UI, ABI de plugin ou format de configuration Notepad++ n’est inclus.

`codeLanguageFromName(idOrName)` accepte IDs, noms et quelques alias, puis retombe sur plain. `codeLanguageForPath(path)` teste les noms exacts puis les extensions sans tenir compte de la casse. Ajoutez un contexte de projet pour les cas ambigus comme les en-têtes Unreal, ou un choix manuel. Les associations de plugins ont priorité ; leur retrait restaure les modes intégrés. Les conflits entre plugins sont déterministes par ID numérique stable.

## Ajouter une syntaxe depuis un plugin {#add-a-syntax-from-an-application-plugin}

```cpp
#include <cyui/modules/Code.h>
cyui::CodeLanguageDefinition syntax;
syntax.id = "my-plugin/config";  // stable, namespaced ID
syntax.name = "My configuration";
syntax.extensions = {"mycfg"}; // no leading dot
cyui::CodeSyntaxRules rules;
rules.keywords = {"option", "section"};
rules.types = {"number", "vector"};
rules.lineComment = "#";
rules.blockCommentStart = "/*";
rules.blockCommentEnd = "*/";
syntax.rules = rules;
const auto language = cyui::registerCodeLanguage(syntax);
// view.setText(source, language, documentRevision);
// editor.setLanguage(language);
// On plugin removal, after stopping calls and before unloading its code:
cyui::unregisterCodeLanguage(syntax.id);
```

Vous pouvez aussi définir `base` sur un mode intégré, ses `wordLists` Lexilla numérotées depuis zéro et des `properties`. Les listes vides héritent de la base. Les règles acceptent un délimiteur de commentaire de ligne, une paire de bloc, des guillemets, échappements et ensembles de mots sensibles ou non à la casse. Pour une grammaire imbriquée ou un parseur spécifique, fournissez `highlight(std::string_view) -> std::vector<CodeToken>`. La priorité est callback, règles, puis moteur de base.

Les tokens utilisent des offsets **en octets UTF-8 bruts**, pas des colonnes visuelles, positions UTF-16 ou tabulations développées. Les plages sont non vides, triées, disjointes, bornées par l’instantané et les limites de points de code. Les espaces non couverts restent neutres. Rôles inconnus et plages invalides déclenchent une exception avant remplacement du document préparé. Les thèmes donnent les couleurs par rôle ; aucun littéral de couleur UI n’est fourni par le plugin. Les définitions et instantanés sont bornés (source : 32 Mio). L’hôte traite les erreurs et limites de temps de son code de plugin.

Réenregistrer un ID de plugin le met à jour sans changer sa valeur numérique. Les valeurs intégrées 0..13 sont conservées ; les valeurs dynamiques utilisent le bit fort. Persistez l’ID texte, pas le numéro dynamique. Les modes intégrés ne peuvent pas être remplacés ; les collisions de hachage sont refusées. Les instantanés du registre sont synchronisés, mais les callbacks s’exécutent hors mutex. Leur durée de vie appartient à l’hôte : le retrait n’attend pas les appels déjà en cours. Arrêtez et joignez ces appels avant de décharger la bibliothèque ou ses services capturés.

## Cache, édition et limites {#caching-editing-and-limits}

CodeDocument met en cache révisions du document et du registre. CodeEditor ne recolore qu’après un changement de texte natif ou de registre. Lexilla relit l’instantané borné entier après une édition : ce n’est pas un parseur incrémental. Une modification pendant le dessin reçoit les nouvelles couleurs à la frame suivante. `refreshSyntax()` prépare explicitement, même sans fenêtre. La préparation ne modifie ni octets, ni curseur, ni historique Undo. `setSyntaxEnabled(false)` conserve les adaptateurs lexicaux/sémantiques d’un hôte plus riche sans imposer un autre éditeur.

Il s’agit de coloration lexicale, sans complétion LSP, refactoring ou sémantique de compilateur. Les modules Python historiques gardent leur implémentation ; cette nouvelle API native de registre et callbacks est C++ et ne fournit pas de liaison Python.

Pour les grands documents servis par l’hôte, utilisez [PagedCodeEditor](doc:paged-code)
dans le même module CodeEditor, sans passer chaque page à `CodeEditor::setText`.
Cette vue distincte conserve des fragments bornés et remet les transactions
protégées par révision au modèle documentaire de l’application. Ses fonctions
d’édition de base diffèrent du TextEditor en mémoire ; consultez ses limites.

## Dépendances embarquées {#bundled-dependencies}

`third_party/code-syntax.lock.json` fige les révisions Lexilla, en-têtes Scintilla, propriétés SciTE et les sommes des archives. Seuls les moteurs Lexilla sélectionnés et leur bibliothèque de support sont compilés, sans l’éditeur Scintilla. Conservez les trois licences et THIRD_PARTY_NOTICES.md.

## Fichiers de journal {#log-files}

`CodeLanguage::log` reconnaît `.log`/`.logs` et colore la ligne entière. INFO et les lignes non marquées gardent le texte normal.

| Mot de niveau, sans sensibilité à la casse | Rôle |
| --- | --- |
| Error, Fatal, Critical, Panic, Exception | error, rouge par défaut |
| Warning, Warn | warning, orange |
| Debug, Trace, Verbose, VeryVerbose | comment, atténué |
| Autres, y compris Info | text, normal |

Seuls les mots complets comptent : ErrorCode, WarningCount et traceback ne sont pas des niveaux. Priorité à l’erreur, puis avertissement, puis verbosité. Les niveaux entre crochets, `LogTemp: Error:`, champs JSON et préfixes horodatés sont pris en compte. Les séquences ANSI CSI sont ignorées pour la détection mais les octets restent intacts. Les lignes visuelles repliées héritent de la couleur. Les continuations de pile sans niveau restent normales ; l’hôte gère les événements multilignes et l’élimination des échappements. Un plugin peut hériter de `CodeLanguage::log` ou fournir son callback.

## Verse {#verse}

`CodeLanguage::verse` / `verse` sélectionne le scanner natif original. `.verse` est reconnu sans tenir compte de la casse, y compris `*.digest.verse`. Aucun SDK UEFN, moteur Lexilla supplémentaire ou serveur de langage n’est lié. CodeEditor et CodeDocument partagent les tokens : mots-clés, types, effets/spécificateurs d’accès, attributs, appels, nombres décimaux/hexadécimaux/exponentiels, chaînes et caractères ont des rôles sémantiques.

Les commentaires `#`, blocs imbriqués `<# ... #>` et commentaires indentés `<#>` gardent leur état entre lignes. L’interpolation des chaînes colore les expressions, accolades imbriquées, chaînes et commentaires ; les accolades échappées restent du texte. Une chaîne ou un commentaire inachevé garde sa couleur jusqu’à la fin. L’imbrication est bornée à 63 niveaux ; au-delà, le reste est affiché comme chaîne. Les octets et Undo ne changent pas.

Un plugin peut hériter de `base = CodeLanguage::verse` ; les ensembles 0/1/2 remplacent mots-clés, types et fonctions, les entrées vides héritent des valeurs par défaut. Règles et callbacks restent prioritaires, et le retrait restaure les associations. La coloration ne fournit ni compilation Verse, ni complétion UEFN, ni analyse sémantique.

Références : [Book of Verse](https://verselang.github.io/book/00_overview/) et [référence Epic](https://dev.epicgames.com/documentation/en-us/fortnite/verse-language-reference).

```image
{"id": "code-view", "src": "assets/code.webp", "alt": "Vue source et syntaxe partagée", "controls": "minimal"}
```
