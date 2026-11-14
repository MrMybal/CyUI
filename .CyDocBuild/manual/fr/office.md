---
id: office
title: Grilles, documents paginés et formats bureautiques
description: Cellules natives, feuilles de calcul, pages et échanges OpenDocument/Open XML.
section: Guides
order: 18
sourceRevision: 1
---

## Choisir les modules {#modules}

Ces modules C++ dessinent des contrôles Dear ImGui natifs. Ils ne nécessitent ni
suite bureautique installée, ni navigateur, ni serveur. L’application fournit les
dialogues de fichiers, le stockage et les textures d’images natives. Les API Python
conservent leur périmètre documenté ; les nouveaux widgets et formats bureautiques
sont actuellement des API C++.

| Cible | Fonction | Dépendances |
| --- | --- | --- |
| `CyUI::DataGrid` | Grille de cellules éditables réutilisable | Core |
| `CyUI::Spreadsheet` | Feuilles, barre de formule et calcul | DataGrid |
| `CyUI::RichDocument` | Éditeur de documents enrichis paginés | DocumentEditor, DataGrid |
| `CyUI::OfficeIO` | Échange de fichiers bureautiques en mémoire | RichDocument, Spreadsheet, DocumentIO, MIT pugixml |

```cmake
set(CYUI_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_GALLERY OFF CACHE BOOL "" FORCE)
set(CYUI_MODULES "DataGrid" CACHE STRING "" FORCE)
add_subdirectory(external/CyUI)
target_link_libraries(app PRIVATE CyUI::DataGrid)
```

Choisissez `OfficeIO` lorsque l’application nécessite les formats de documents et
classeurs. Ses dépendances sont liées automatiquement. DataGrid seul ne compile
ni Markdown, ni éditeur source, ni XML, ni ZIP, ni Web/CEF, ni moteur de formules.
Sans CMake, ajoutez les `.cpp`, dépendances et macros des modules sélectionnés.
OfficeIO compile aussi `third_party/pugixml/pugixml.cpp` ; DocumentIO compile
miniz, comme indiqué dans le guide du Markdown natif.

## Modifier les cellules et tableaux {#grid}

```image
{"id":"office-grid","src":"assets/office-grid-fr.webp","alt":"Grille native éditable avec actions sur les lignes et colonnes","controls":"minimal"}
```

```cpp
#include <cyui/modules/DataGrid.h>
cyui::GridModel cells(20, 6);
cyui::GridState selection;
cells.paste({0, 0}, "Element\tQuantite\nTexture\t4");
if (cyui::dataGrid("inventory", selection, cells, {0, 420}).changed)
    markDirty();
```

Clic/Maj-clic ou glissement sélectionnent une plage rectangulaire. Les flèches
déplacent la cellule courante ; Maj étend la sélection. Double-clic, F2 ou Entrée
commencent l’édition. Entrée valide et descend ; Tab/Maj+Tab valide et change de
cellule ; Échap annule la saisie. Ctrl/Cmd+C/V copie/colle une grille TSV. Les
cellules entre guillemets conservent tabulations, guillemets et retours à la ligne.
Les API `copy`/`paste` acceptent aussi le CSV avec une virgule ; elles n’exécutent
pas le texte des cellules.

La barre et le menu contextuel ajoutent, insèrent ou suppriment lignes/colonnes.
Le formatage comprend gras, italique, données souligné/barré, alignement et
précision décimale. Les poignées natives redimensionnent les colonnes ; leurs
largeurs restent un état de vue, pas un modèle de mise en page bureautique.
Collages et changements de structure sont atomiques et annulables. Une vue en
lecture seule ne modifie pas le modèle, qui reste sous le contrôle de l’application.

Les tableaux Markdown Direct réutilisent cette grille et l’historique source.
Leurs cellules acceptent du Markdown en ligne ; Lecture affiche le résultat
formaté. Markdown n’encode ni formules de classeur, ni mise en page, ni tous les
formats de cellules.

## Feuilles et formules {#spreadsheet}

```image
{"id":"office-spreadsheet","src":"assets/office-spreadsheet-fr.webp","alt":"Classeur natif avec barre de formule, totaux et onglets de feuilles","controls":"minimal"}
```

```cpp
#include <cyui/modules/Spreadsheet.h>
cyui::Workbook workbook;
cyui::SpreadsheetState view;
auto& sheet = workbook.sheets[0];
sheet.name = "Budget";
sheet.grid.setText(0, 0, "12.5");
sheet.grid.setText(0, 1, "4");
sheet.grid.setText(0, 2, "=A1*B1");
cyui::spreadsheet("budget", view, workbook, {0, 480});
```

La barre de formule modifie le texte d’origine, tandis que la grille affiche les
valeurs calculées. Ajoutez, renommez ou fermez les onglets ; l’annulation du
classeur couvre les cellules et la structure. Le copier-coller interne conserve
types/formats et translate les références relatives en respectant `$A$1`, `$A1`
et `A$1`. Un presse-papiers externe est du TSV simple, sans métadonnées enrichies.

Le moteur borné prend en charge `+ - * / ^`, comparaisons, concaténation `&`,
pourcentages, références/plages A1, noms de feuilles entre apostrophes, références
entre feuilles, `SUM`, `AVERAGE`, `MIN`, `MAX`, `COUNT`, `COUNTA`, `IF`, `ABS` et
`ROUND`. Les noms de fonctions sont anglais, indépendamment de la langue de
l’interface. Références et séparateurs OpenFormula sont traduits pour ODS/FODS.
Les changements de structure décalent les références ; une cellule référencée
supprimée devient `#REF!`.

Cycles, divisions par zéro, fonctions inconnues et limites de calcul produisent
des erreurs visibles. La source des formules inconnues reste conservée pour une
autre application. Le catalogue complet Excel/OpenFormula, les formules
matricielles, plages nommées, plages déversées, données externes et macros ne sont
pas implémentés.

## Documents enrichis paginés {#writer}

```image
{"id":"office-writer","src":"assets/office-writer-fr.webp","alt":"Document paginé natif avec paragraphes enrichis, tableau et image","controls":"minimal"}
```

```cpp
#include <cyui/modules/RichDocument.h>
cyui::RichDocumentEditor editor;
auto document = cyui::richDocumentFromMarkdown("# Rapport\n\nDu **texte riche**.");
document.page.leftMM = 24;
document.page.header = "Rapport du projet";
document.page.footer = "Page {page} / {pages}";
editor.setDocument(document);
if (editor.draw("report", {0, 640}))
    saveDraft(editor.document());
```

Les pages utilisent dimensions physiques, marges, retours natifs, tailles de texte,
alignement, interligne, images, tableaux, sauts de page et champs de numérotation
des en-têtes/pieds de page. Le zoom de travail ne modifie pas les dimensions
enregistrées. Double-cliquez un paragraphe, tableau ou une image pour ouvrir son
éditeur de bloc natif. Les paragraphes réutilisent DocumentEditor ; les tableaux,
DataGrid. Appliquer crée une transaction ; Annuler abandonne le brouillon du bloc.
L’application peut appeler `selectBlock`, `editBlock`, `setZoom`, `undo` et `redo`.

L’espace paginé fonctionne par blocs : il ne propose pas de sélection/caret continu
entre plusieurs pages. Sa pagination utilise la police de l’application et ne
garantit pas les mêmes coupures que Writer/Word. Les lignes de tableaux ont un
aperçu compact ; leurs contenus longs/multilignes restent éditables dans la grille.
Les largeurs de colonnes sont normalisées à l’export bureautique. La conversion
Markdown est un utilitaire qui ne conserve pas la mise en page ; utilisez le
modèle riche et un format bureautique pour cela.

## Échanger avec LibreOffice et Microsoft {#formats}

| Contenu | Formats lus et écrits |
| --- | --- |
| Documents enrichis | ODT, DOCX, FODT plat |
| Classeurs | ODS, XLSX, FODS plat |

ODT/ODS sont les formats OpenDocument employés par Writer/Calc. DOCX/XLSX utilisent
Open XML. Les extensions de modèles peuvent être reconnues pour importer leur
contenu ; les exports produisent des documents ordinaires, pas des modèles.
Les anciens DOC/XLS binaires, RTF, ODP/PPTX, chiffrement et mots de passe ne sont
pas pris en charge.

```cpp
#include <cyui/modules/OfficeIO.h>
auto imported = cyui::readOfficeDocument(fileBytes, cyui::OfficeFormat::odt);
showConversionIssues(imported.issues);
editor.setDocument(imported.document);
std::vector<cyui::OfficeIssue> issues;
auto output = cyui::writeOfficeDocument(editor.document(),
    cyui::OfficeFormat::docx, &issues);
```

Les classeurs utilisent `readOfficeWorkbook`/`writeOfficeWorkbook`, avec `ods` ou
`xlsx`. Texte, enrichissement direct courant, propriétés de paragraphes/pages,
champs simples des en-têtes/pieds, tableaux rectangulaires, images embarquées,
noms de feuilles, cellules textuelles/numériques, formats courants et source des
formules possèdent une représentation native. Les octets d’images ODT/DOCX
reviennent dans `document.assets` ; enregistrez leurs clés dans votre fournisseur
de textures natives. Aucune ressource n’est extraite ou téléchargée automatiquement.
L’export d’images FODT plat signale une limite ; utilisez ODT pour les embarquer.

Inspectez `OfficeIssue` avant d’enregistrer une conversion. Parmi les limites
reconnues : styles Word nommés, typographie arbitraire, cellules fusionnées,
numérotation complexe, révisions, commentaires/notes, graphiques, tableaux croisés,
liens externes et objets incorporés. Certains éléments deviennent du texte/valeurs
visibles ; d’autres sont omis. `OfficeOptions::strict = true` refuse les conversions
avec pertes reconnues. Le rapport décrit les écarts détectés sans garantir une
fidélité exhaustive. Gardez l’original si l’application exige une édition sans
perte de fichiers arbitraires.

## Limites et vérifications {#validation}

Grille : 100000 lignes, 256 colonnes, un million de cellules, 32 Mio de texte et
1 Mio par cellule. Un classeur accepte 64 feuilles et un million de cellules au
total. Le modèle dense vise les outils d’interface, pas tout l’espace d’adressage
d’Excel. Les répétitions ODF vides finales sont ramenées à la plage utilisée.
Les historiques sont bornés ; les très gros instantanés peuvent dépasser leur budget.

OfficeIO réutilise les budgets ZIP de DocumentIO (archive 64 Mio, contenu 128 Mio,
32 Mio/entrée, 4096 entrées), refuse les DTD, limite profondeur/nombre de nœuds XML
et ne résout aucune entité externe. Parsing des formules et dépendances possèdent
aussi des budgets. LibreOffice sert aux vérifications, sans devenir une dépendance
d’exécution de la bibliothèque.

```powershell
build_demo.bat
build\cyui_demo.exe --gallery-only --tab 26 --locale fr
ctest --test-dir build/office-final --output-on-failure
python -B tests/validate_office_interop.py --exe build/office-final/cyui_test_office.exe
```

Les tests natifs couvrent saisie, presse-papiers, transactions, références, cycles,
formats, pagination, édition de blocs et échanges des six formats. Le script
optionnel fait passer les données synthétiques dans LibreOffice avec un profil
isolé, puis vérifie textes, tableaux, champs de page et totaux calculés. Windows
et LibreOffice installé sont exercés localement. Linux/macOS, Unreal Editor et
la fidélité de documents tiers arbitraires nécessitent encore une validation cible.

Références : [schéma OpenDocument 1.3](https://docs.oasis-open.org/office/OpenDocument/v1.3/OpenDocument-v1.3-part3-schema.html),
[feuilles SpreadsheetML](https://learn.microsoft.com/en-us/office/open-xml/spreadsheet/working-with-sheets).
