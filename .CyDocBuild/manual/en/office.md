---
id: office
title: Grids, paginated documents and Office formats
description: Native cell editing, worksheets, document pages and OpenDocument/Open XML exchange.
section: Guides
order: 18
revision: 1
---

## Choose the modules {#modules}

These C++ modules draw native Dear ImGui controls. They do not require an installed
office application, a browser or a server. The host supplies file dialogs, storage
and native image textures. The Python document APIs keep their documented scope;
these new Office widgets and file APIs are currently C++ only.

| Target | Purpose | Dependencies |
| --- | --- | --- |
| `CyUI::DataGrid` | Reusable editable cell grid | Core |
| `CyUI::Spreadsheet` | Worksheets, formula bar and calculation | DataGrid |
| `CyUI::RichDocument` | Native paginated rich document editor | DocumentEditor, DataGrid |
| `CyUI::OfficeIO` | Office file exchange in memory | RichDocument, Spreadsheet, DocumentIO, MIT pugixml |

```cmake
set(CYUI_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_GALLERY OFF CACHE BOOL "" FORCE)
set(CYUI_MODULES "DataGrid" CACHE STRING "" FORCE)
add_subdirectory(external/CyUI)
target_link_libraries(app PRIVATE CyUI::DataGrid)
```

Choose `OfficeIO` instead when the application needs both document/spreadsheet
formats. Its dependencies are linked automatically. Selecting DataGrid alone does
not compile Markdown, the source editor, XML, ZIP, Web, CEF or a formula engine.
Without CMake, add each selected module's `.cpp`, its dependencies and feature
defines. OfficeIO additionally compiles `third_party/pugixml/pugixml.cpp`;
DocumentIO compiles miniz as described in the native Markdown guide.

## Edit cells and tables {#grid}

```image
{"id":"office-grid","src":"assets/office-grid.webp","alt":"Native editable grid with row and column actions","controls":"minimal"}
```

```cpp
#include <cyui/modules/DataGrid.h>
cyui::GridModel cells(20, 6);
cyui::GridState selection;
cells.paste({0, 0}, "Item\tCount\nTexture\t4");
if (cyui::dataGrid("inventory", selection, cells, {0, 420}).changed)
    markDirty();
```

Click/Shift-click or drag to select a rectangular range. Arrows move the current
cell; Shift extends the range. Double-click, F2 or Enter starts editing. Enter
commits and moves down; Tab/Shift+Tab commits and moves between cells; Escape
cancels the draft. Ctrl/Cmd+C/V copies/pastes a TSV grid. Quoted cells preserve
embedded tabs, quotes and line breaks. Programmatic `copy`/`paste` also accept CSV
with a comma separator; they do not execute cell text.

Use the toolbar/context menu to add, insert or remove rows and columns. Formatting
supports bold, italic, underline/strike data, alignment and decimal precision.
Columns resize with native table handles; their widths are view state, not an
Office layout model. Models support atomic paste/structure edits and undo/redo.
Read-only views do not change the model. Application data remains under host control.

Live Markdown tables reuse this grid and the document's source history. Their
cells accept inline Markdown; Reading shows the formatted result. Markdown does
not encode workbook formulas, page properties or arbitrary cell formatting.

## Worksheets and formulas {#spreadsheet}

```image
{"id":"office-spreadsheet","src":"assets/office-spreadsheet.webp","alt":"Native workbook with a formula bar, totals and sheet tabs","controls":"minimal"}
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

The formula bar edits the original cell text while the grid displays calculated
values. Add, rename or close sheet tabs; workbook undo covers grid edits and
structure changes. Native internal copy/paste retains cell types/formats and
translates relative A1 references, preserving `$A$1`, `$A1` and `A$1` anchors.
External clipboard input is plain TSV and cannot preserve rich cell metadata.

The bounded evaluator supports arithmetic `+ - * / ^`, comparisons, concatenation
`&`, percentages, A1 references/ranges, quoted sheet names, cross-sheet references,
`SUM`, `AVERAGE`, `MIN`, `MAX`, `COUNT`, `COUNTA`, `IF`, `ABS` and `ROUND`.
Function names use English, independent of the interface language. OpenFormula
references/separators are translated for ODS/FODS exchange. Structural row/column
edits shift references; deleted referenced cells become `#REF!`.

Cycles, division by zero, unknown functions and calculation limits are visible
errors. Unknown formula source is retained for another spreadsheet application.
The evaluator does not implement the complete Excel/OpenFormula function catalog,
array formulas, named ranges, dynamic spill, external data or macros.

## Paginated rich documents {#writer}

```image
{"id":"office-writer","src":"assets/office-writer.webp","alt":"Native paginated document with formatted paragraphs, a table and an image","controls":"minimal"}
```

```cpp
#include <cyui/modules/RichDocument.h>
cyui::RichDocumentEditor editor;
auto document = cyui::richDocumentFromMarkdown("# Report\n\nSome **rich text**.");
document.page.leftMM = 24;
document.page.header = "Project report";
document.page.footer = "Page {page} / {pages}";
editor.setDocument(document);
if (editor.draw("report", {0, 640}))
    saveDraft(editor.document());
```

Pages use physical dimensions and margins, native text wrapping, paragraph sizes,
alignment, line spacing, images, tables, explicit page breaks and header/footer
page fields. The workspace zoom does not change saved physical dimensions.
Double-click a paragraph, table or image to open its native block editor. Paragraph
editing reuses DocumentEditor; table editing reuses DataGrid. Apply commits one
document transaction; Cancel discards that block draft. Host actions can use
`selectBlock`, `editBlock`, `setZoom`, `undo` and `redo`.

This is a block-based paginated workspace. It does not provide a continuous text
selection/caret across pages. Native pagination uses the current host font, so it
does not promise the exact line/page breaks of Writer or Word. Table rows use a
compact preview; large/multiline cell contents remain editable in the grid.
Column widths normalize in Office output. Markdown conversion is a convenience
and cannot retain page settings. Use the rich model and an Office format for that.

## Exchange LibreOffice and Microsoft files {#formats}

| Content | Read/write formats |
| --- | --- |
| Rich documents | ODT, DOCX, flat FODT |
| Workbooks | ODS, XLSX, flat FODS |

ODT/ODS are OpenDocument formats used by LibreOffice Writer/Calc. DOCX/XLSX use
Open XML. Template extensions can be recognized for importing their content;
these writers produce ordinary documents, not template packages. Legacy binary
DOC/XLS, RTF, ODP/PPTX, encryption and password-protected files are unsupported.

```cpp
#include <cyui/modules/OfficeIO.h>
auto imported = cyui::readOfficeDocument(fileBytes, cyui::OfficeFormat::odt);
showConversionIssues(imported.issues);
editor.setDocument(imported.document);
std::vector<cyui::OfficeIssue> issues;
auto output = cyui::writeOfficeDocument(editor.document(),
    cyui::OfficeFormat::docx, &issues);
```

Workbooks use `readOfficeWorkbook`/`writeOfficeWorkbook` with `ods` or `xlsx`.
Text, common direct emphasis, basic paragraph/page properties, plain header/footer
fields, editable rectangular tables, packaged images, sheet names, cell text/types,
common formats and formula source have native representations. ODT/DOCX package
image bytes are returned in `document.assets`; register them with a native image
provider using their resource keys. No resource is extracted or fetched automatically.
Flat FODT image export reports an issue; use packaged ODT for embedded images.

Inspect `OfficeIssue` before saving a converted file. Recognized unsupported
features include named Word style definitions, arbitrary typography, merged cells,
complex numbering, revision history, comments/notes, charts, pivots, external
links and embedded objects. Some are flattened to visible values/text; others are
omitted. `OfficeOptions::strict = true` rejects recognized lossy conversion.
The issue list documents detected gaps, not a guarantee of complete fidelity.
Keep the original when an application requires lossless editing of arbitrary files.

## Limits and verification {#validation}

Grid limits: 100000 rows, 256 columns, one million cells, 32 MiB text and 1 MiB per
cell. Workbooks accept at most 64 sheets and one million total cells. Dense models
are intended for UI tools, not the entire address space of an Excel workbook.
Imported repeated trailing empty ODF rows/columns are trimmed to the used range.
Histories are bounded; very large snapshots may exceed the undo budget.

OfficeIO reuses DocumentIO's ZIP budgets (64 MiB archive, 128 MiB expanded,
32 MiB/entry, 4096 entries), rejects DTDs, limits XML depth/nodes and never resolves
external entities. Formula parsing/dependency traversal have explicit budgets.
The Office application used for validation is not a runtime dependency.

```powershell
build_demo.bat
build\cyui_demo.exe --gallery-only --tab 26 --locale en
ctest --test-dir build/office-final --output-on-failure
python -B tests/validate_office_interop.py --exe build/office-final/cyui_test_office.exe
```

Native tests cover cell typing, clipboard parsing, transactions, formula references,
cycles, formatting, pagination, block editing and all six file roundtrips. The
optional interop script exchanges synthetic files through installed LibreOffice
with an isolated profile, then checks text, tables, page fields and calculated totals.
Windows and the installed LibreOffice are exercised locally. Linux/macOS runtime,
Unreal Editor and arbitrary third-party document fidelity still require target testing.

References: [OpenDocument 1.3 schema](https://docs.oasis-open.org/office/OpenDocument/v1.3/OpenDocument-v1.3-part3-schema.html),
[SpreadsheetML sheets](https://learn.microsoft.com/en-us/office/open-xml/spreadsheet/working-with-sheets).
