#include <cyui/modules/OfficeIO.h>
#include <imgui_internal.h>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <iterator>
#include <clocale>
#include <limits>

using namespace cyui;
namespace {
int failures = 0;
void check(bool v, const char *message) {
    if (!v) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
}
void save(const std::filesystem::path &path, const std::vector<unsigned char> &bytes) {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
}
std::vector<unsigned char> load(const char *path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("Missing input file");
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
std::string plain(const RichDocumentData &d) {
    std::string out;
    for (const auto &b : d.blocks) {
        for (const auto &r : b.runs)
            out += r.text;
        out += '\n';
    }
    return out;
}
template <class F> void frame(F draw) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({1000, 800});
    ImGui::Begin("Office tests");
    if (GImGui->OpenPopupStack.empty())
        ImGui::SetWindowFocus();
    draw();
    ImGui::End();
    ImGui::Render();
}
} // namespace
int main(int argc, char **argv) {
    try {
        if (argc == 4 && std::string(argv[1]) == "--convert") {
            OfficeFormat source{}, target{};
            if (!officeFormatFromExtension(argv[2], source) || !officeFormatFromExtension(argv[3], target))
                return 2;
            auto bytes = load(argv[2]);
            if (source == OfficeFormat::ods || source == OfficeFormat::xlsx || source == OfficeFormat::fods) {
                auto parsed = readOfficeWorkbook(bytes, source);
                save(argv[3], writeOfficeWorkbook(parsed.workbook, target));
                auto values = evaluateWorkbook(parsed.workbook);
                std::printf("Sheets %zu, first %s, issues %zu\n", parsed.workbook.sheets.size(),
                            parsed.workbook.sheets[0].name.c_str(), parsed.issues.size());
            } else {
                auto parsed = readOfficeDocument(bytes, source);
                save(argv[3], writeOfficeDocument(parsed.document, target));
                std::printf("Blocks %zu, issues %zu\n", parsed.document.blocks.size(), parsed.issues.size());
            }
            return 0;
        }
        GridModel grid(2, 2);
        grid.paste({0, 0}, "Name\tValue\n\"line 1\nline 2\"\t\"a\"\"b\"");
        check(grid.cell(1, 0).text == "line 1\nline 2" && grid.cell(1, 1).text == "a\"b",
              "quoted multiline TSV paste");
        const auto source = grid.copy({{0, 0}, {1, 1}});
        GridModel copied(1, 1);
        copied.paste({0, 0}, source);
        check(copied.copy({{0, 0}, {1, 1}}) == source, "rectangular clipboard roundtrip");
        bool bad = false;
        try {
            grid.paste({0, 0}, "\"unterminated");
        } catch (const std::invalid_argument &) {
            bad = true;
        }
        check(bad && grid.copy({{0, 0}, {1, 1}}) == source, "failed paste leaves data unchanged");
        grid.insertColumns(1);
        grid.setText(0, 1, "Inserted");
        grid.undo();
        grid.undo();
        check(grid.columns() == 2, "structural grid undo");
        grid.redo();
        check(grid.columns() == 3, "structural grid redo");
        grid.redo();
        check(grid.cell(0, 1).text == "Inserted", "cell delta survives structural undo and redo");
        {
            GridModel large(10000, 100);
            GridCell label;
            label.text = "=A1";
            label.literal = true;
            label.decimals = 3;
            large.setCell(9999, 99, label);
            large.setText(9999, 99, "42");
            check(large.undo() && large.cell(9999, 99).literal && large.cell(9999, 99).text == "=A1" &&
                      large.cell(9999, 99).decimals == 3,
                  "single-cell undo on a million-cell grid restores the entire previous cell");
            const auto revision = large.revision();
            check(!large.setCell(9999, 99, label) && revision == large.revision() && large.canRedo(),
                  "no-op preserves revision and redo");
            check(large.redo() && !large.cell(9999, 99).literal && large.cell(9999, 99).text == "42",
                  "large grid cell redo");
            large.undo();
            large.setText(0, 0, "New branch");
            check(!large.canRedo() && large.undo() && large.undo() && large.cell(9999, 99).text.empty(),
                  "new edits discard redo and preserve older cell deltas");
        }
        {
            GridModel bounded(33, 1);
            const std::string megabyte(1024 * 1024, 'x');
            for (std::size_t r = 0; r < 32; ++r)
                bounded.setText(r, 0, megabyte);
            const auto revision = bounded.revision();
            bool rejected = false;
            try {
                bounded.setText(32, 0, "x");
            } catch (const std::length_error &) {
                rejected = true;
            }
            check(rejected && bounded.revision() == revision && bounded.cell(32, 0).text.empty(),
                  "cell delta enforces aggregate text limit atomically");
            check(bounded.undo() && bounded.setText(32, 0, megabyte) && !bounded.canRedo(),
                  "undo releases the previous cell's text quota");
        }
        Workbook book;
        book.sheets[0].name = "Budget";
        auto &m = book.sheets[0].grid;
        m.resize(6, 4);
        m.paste(
            {0, 0},
            "Item\tPrice\tQuantity\tTotal\nCable\t12.5\t4\t=B2*C2\nMicrophone\t99\t2\t=B3*C3\nTotal\t\t\t="
            "SUM(D2:D3)\nCondition\t\t\t=IF(D4>200,ROUND(D4/3,2),0)\nLiteral\t001\t\t=COUNT(B2:C3)");
        auto literal = m.cell(5, 1);
        literal.literal = true;
        m.setCell(5, 1, literal);
        m.format({{0, 0}, {0, 3}}, 1);
        SpreadsheetSheet rates;
        rates.name = "Résumé";
        rates.grid.resize(3, 2);
        rates.grid.setText(0, 0, "=Budget!D4");
        rates.grid.setText(1, 0, "=SUM(Budget!B2:Budget!B3)");
        rates.grid.setText(2, 0, "=IF(0,1/0,7)");
        book.sheets.push_back(rates);
        auto values = evaluateWorkbook(book);
        check(values[0][1 * 4 + 3].number == 50 && values[0][3 * 4 + 3].number == 248,
              "arithmetic and SUM formulas");
        check(std::abs(values[0][4 * 4 + 3].number - 82.67) < .001 && values[1][0].number == 248 &&
                  values[1][4].number == 7,
              "ROUND, IF and cross-sheet references");
        check(values[0][5 * 4 + 1].display() == "001" && !values[0][5 * 4 + 1].numeric,
              "imported numeric-looking text remains literal");
        Workbook cycle;
        cycle.sheets[0].grid.setText(0, 0, "=B1");
        cycle.sheets[0].grid.setText(0, 1, "=A1");
        check(evaluateWorkbook(cycle)[0][0].error == "#CYCLE!", "formula dependency cycle detection");
        Workbook structural = book;
        check(spreadsheetStructure(structural, 0, 1, 1), "insert workbook row");
        check(structural.sheets[0].grid.cell(2, 3).text == "=B3*C3" &&
                  structural.sheets[1].grid.cell(0, 0).text == "=Budget!D5",
              "structural edits adjust local and cross-sheet formulas");
        spreadsheetStructure(structural, 0, -1, 2);
        check(structural.sheets[0].grid.cell(3, 3).text.find("#REF!") != std::string::npos,
              "deleted formula references report REF");
        {
            Workbook ranges;
            ranges.sheets[0].name = "Taux d'été";
            ranges.sheets.emplace_back();
            ranges.sheets[0].grid.setText(0, 0, "10");
            ranges.sheets[0].grid.setText(1, 0, "20");
            ranges.sheets[1].grid.setText(0, 0, "=SUM('Taux d''été'!$A$1 : $A$2)+A2");
            GridCell label;
            label.text = "='Taux d''été'!A1";
            label.literal = true;
            ranges.sheets[1].grid.setCell(0, 1, label);
            ranges.sheets[1].grid.setText(0, 2, "='Taux d''été'!A1&\"[A1]\"");
            check(spreadsheetStructure(ranges, 0, 1, 0) &&
                      ranges.sheets[1].grid.cell(0, 0).text == "=SUM('Taux d''été'!$A$2 : $A$3)+A2" &&
                      evaluateWorkbook(ranges)[1][0].number == 30,
                  "second endpoint inherits the sheet, absolute anchors move, independent reference stays local");
            check(spreadsheetStructure(ranges, 0, 2, 0) &&
                      ranges.sheets[1].grid.cell(0, 0).text == "=SUM('Taux d''été'!$B$2 : $B$3)+A2" &&
                      evaluateWorkbook(ranges)[1][0].number == 30,
                  "cross-sheet range follows column insertion");
            check(spreadsheetRenameSheet(ranges, 0, "L'année") &&
                      ranges.sheets[1].grid.cell(0, 0).text == "=SUM('L''année'!$B$2 : $B$3)+A2" &&
                      evaluateWorkbook(ranges)[1][0].number == 30,
                  "sheet rename preserves quoted names and inherited range endpoint");
            check(ranges.sheets[1].grid.cell(0, 1).text == label.text &&
                      ranges.sheets[1].grid.cell(0, 1).literal,
                  "literal cells survive structural edits and sheet renaming byte for byte");
            check(ranges.sheets[1].grid.cell(0, 2).text == "='L''année'!B2&\"[A1]\"",
                  "brackets in formula text literals leave ordinary references eligible for updates");
            check(!spreadsheetRenameSheet(ranges, 0, ranges.sheets[1].name) &&
                      ranges.sheets[0].name == "L'année",
                  "duplicate sheet rename leaves workbook intact");
        }
        check(spreadsheetFormulaFromODF("of:=SUM([.B2:.C3]);\"a;b\"") == "=SUM(B2:C3),\"a;b\"",
              "OpenFormula separators respect strings");
        check(spreadsheetFormulaFromODF(spreadsheetFormulaToODF("=SUM(B2:C3)+'Résumé'!A1")) ==
                  "=SUM(B2:C3)+'Résumé'!A1",
              "ODF A1 range and sheet formula conversion");
        check(spreadsheetTranslateFormula("=A1+$B1+C$2+$D$3+SUM(A2:B4)+\"A1\"", 2, 1) ==
                  "=B3+$B3+D$2+$D$3+SUM(B4:C6)+\"A1\"",
              "relative copy shifts references and retains absolute anchors and string literals");
        book.sheets[0].grid.format({{1, 1}, {2, 1}}, 0, GridAlignment::right, 2);
        RichDocumentData document = richDocumentFromMarkdown(
            "# Native office documents\n\nA **bold** paragraph with *italic*, <u>underline</u> and a "
            "[link](https://example.com).\n\n| Item | Count |\n| --- | --- |\n| Native table | 3 |\n");
        RichBlock picture;
        picture.kind = RichBlockKind::image;
        picture.image = "assets/test.png";
        picture.caption = "Synthetic image";
        picture.imageWidthMM = 30;
        picture.imageHeightMM = 30;
        document.blocks.push_back(picture);
        document.assets.push_back(
            {"assets/test.png",
             {137, 80,  78,  71,  13,  10,  26,  10,  0,   0,   0,   13,  73,  72, 68, 82,  0,   0,   0,
              16,  0,   0,   0,   16,  8,   2,   0,   0,   0,   144, 145, 104, 54, 0,  0,   0,   35,  73,
              68,  65,  84,  120, 156, 99,  180, 169, 216, 194, 64,  10,  96,  34, 73, 53,  195, 168, 6,
              226, 0,   19,  145, 234, 224, 96,  84,  3,   49,  128, 228, 80,  2,  0,  127, 132, 1,   136,
              112, 110, 108, 12,  0,   0,   0,   0,   73,  69,  78,  68,  174, 66, 96, 130}});
        document.page.header = "CyUI synthetic document";
        document.page.footer = "Page {page} / {pages}";
        document.page.leftMM = 24;
        RichBlock page;
        page.kind = RichBlockKind::pageBreak;
        document.blocks.push_back(page);
        RichBlock last;
        last.runs = {{DocumentInlineKind::text, "Second page — résumé 😀"}};
        document.blocks.push_back(last);
        std::filesystem::path output = argc > 1 ? argv[1] : "build/office-validation";
        std::filesystem::create_directories(output);
        for (auto format : {OfficeFormat::odt, OfficeFormat::docx, OfficeFormat::fodt}) {
            auto bytes = writeOfficeDocument(document, format);
            auto imported = readOfficeDocument(bytes, format);
            check(plain(imported.document).find("Second page — résumé 😀") != std::string::npos,
                  "office text Unicode roundtrip");
            check(std::abs(imported.document.page.leftMM - 24) < .1, "office page margins roundtrip");
            check(imported.document.page.footer.find("{page}") != std::string::npos &&
                      imported.document.page.footer.find("{pages}") != std::string::npos,
                  "office header/footer page fields roundtrip");
            bool pageBreak = false;
            for (const auto &block : imported.document.blocks)
                pageBreak |= block.kind == RichBlockKind::pageBreak;
            check(pageBreak, "office explicit page break roundtrip");
            bool table = false, bold = false;
            for (const auto &b : imported.document.blocks) {
                table |= b.kind == RichBlockKind::table && b.table.cell(1, 0).text == "Native table";
                for (const auto &r : b.runs)
                    bold |= (r.style & documentBold) != 0;
            }
            check(table && bold, "office table and emphasis roundtrip");
            if (format != OfficeFormat::fodt)
                check(!imported.document.assets.empty(), "packaged native image bytes roundtrip");
            const char *ext = format == OfficeFormat::odt    ? "odt"
                              : format == OfficeFormat::docx ? "docx"
                                                             : "fodt";
            save(output / (std::string("sample.") + ext), bytes);
        }
        for (auto format : {OfficeFormat::ods, OfficeFormat::xlsx, OfficeFormat::fods}) {
            auto bytes = writeOfficeWorkbook(book, format);
            auto imported = readOfficeWorkbook(bytes, format);
            auto calculated = evaluateWorkbook(imported.workbook);
            check(imported.workbook.sheets.size() == 2 && imported.workbook.sheets[1].name == "Résumé",
                  "workbook sheets and Unicode names roundtrip");
            check(calculated[0][3 * 4 + 3].number == 248 && calculated[1][0].number == 248,
                  "office formula roundtrip retains calculation");
            check(imported.workbook.sheets[0].grid.cell(1, 1).decimals == 2,
                  "office decimal format roundtrip");
            check(imported.workbook.sheets[0].grid.cell(5, 1).literal &&
                      calculated[0][5 * 4 + 1].display() == "001",
                  "office text cell type roundtrip");
            const char *ext = format == OfficeFormat::ods    ? "ods"
                              : format == OfficeFormat::xlsx ? "xlsx"
                                                             : "fods";
            save(output / (std::string("sample.") + ext), bytes);
        }
        Workbook unsupported;
        unsupported.sheets[0].grid.setText(0, 0, "=UNSUPPORTED(A1)");
        std::vector<OfficeIssue> issues;
        writeOfficeWorkbook(unsupported, OfficeFormat::ods, &issues);
        check(!issues.empty(), "unsupported formulas produce a conversion issue");
        bad = false;
        try {
            OfficeOptions strict;
            strict.strict = true;
            writeOfficeWorkbook(unsupported, OfficeFormat::xlsx, nullptr, strict);
        } catch (const std::invalid_argument &) {
            bad = true;
        }
        check(bad, "strict Office conversion refuses loss");
        const std::string previousLocale = std::setlocale(LC_NUMERIC, nullptr);
        if (std::setlocale(LC_NUMERIC, "French_France.1252") || std::setlocale(LC_NUMERIC, "fr_FR.UTF-8")) {
            Workbook localeBook;
            localeBook.sheets[0].grid.setText(0, 0, "=1.5*2");
            check(evaluateWorkbook(localeBook)[0][0].number == 3,
                  "formulas are independent of process numeric locale");
            auto localeFile = writeOfficeWorkbook(book, OfficeFormat::ods);
            auto localeResult = readOfficeWorkbook(localeFile, OfficeFormat::ods);
            check(evaluateWorkbook(localeResult.workbook)[0][7].number == 50,
                  "Office numbers use invariant decimal syntax");
        }
        std::setlocale(LC_NUMERIC, previousLocale.c_str());
        for (const std::string xml :
             std::vector<std::string>{"<!DOCTYPE x [<!ENTITY x SYSTEM 'file:///synthetic-never-read'>]><x/>",
                                      std::string(1, '<') + std::string(200, 'x') + '>'}) {
            bad = false;
            try {
                readOfficeDocument(std::vector<unsigned char>(xml.begin(), xml.end()), OfficeFormat::fodt);
            } catch (const std::exception &) {
                bad = true;
            }
            check(bad, "malformed or DTD Office XML is rejected");
        }
        std::string deep;
        for (int n = 0; n < 140; ++n)
            deep += "<x>";
        for (int n = 0; n < 140; ++n)
            deep += "</x>";
        bad = false;
        try {
            readOfficeDocument(std::vector<unsigned char>(deep.begin(), deep.end()), OfficeFormat::fodt);
        } catch (const std::length_error &) {
            bad = true;
        }
        check(bad, "Office XML nesting has a bounded depth");
        auto invalidPage = document;
        invalidPage.blocks[0].paragraph.indent = std::numeric_limits<float>::quiet_NaN();
        bad = false;
        try {
            validateRichDocument(invalidPage);
        } catch (const std::invalid_argument &) {
            bad = true;
        }
        check(bad, "invalid native paragraph dimensions are rejected");
        ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {1000, 800};
        io.DeltaTime = 1.f / 60;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;
        apply();
        // Headless modern texture mode: leave pending atlas textures without a GPU ID.
        GridState state;
        state.hasSelection = true;
        GridModel typing(3, 3);
        GridOptions options;
        options.toolbar = false;
        auto drawGrid = [&] { dataGrid("typing", state, typing, {800, 500}, options); };
        frame(drawGrid);
        io.AddKeyEvent(ImGuiKey_F2, true);
        frame(drawGrid);
        io.AddKeyEvent(ImGuiKey_F2, false);
        frame(drawGrid);
        io.AddInputCharactersUTF8("Edited cell");
        frame(drawGrid);
        io.AddKeyEvent(ImGuiKey_Enter, true);
        frame(drawGrid);
        io.AddKeyEvent(ImGuiKey_Enter, false);
        frame(drawGrid);
        check(typing.cell(0, 0).text == "Edited cell", "native grid keyboard input commits a cell");
        typing.undo();
        check(typing.cell(0, 0).text.empty(), "native grid edit is undoable");
        DocumentEditor markdownTable;
        const std::string tableSource = "| A | B |\n| --- | --- |\n| one | two |\n";
        markdownTable.setMarkdown(tableSource);
        DocumentEditorOptions tableOptions;
        tableOptions.toolbar = false;
        tableOptions.modeSelector = false;
        auto drawTable = [&] { markdownTable.draw("markdown-table", {850, 500}, tableOptions); };
        frame(drawTable);
        // Document cells start immediately: Live hides spreadsheet letters and its toolbar.
        io.AddMousePosEvent(100, 45);
        io.AddMouseButtonEvent(0, true);
        frame(drawTable);
        io.AddMouseButtonEvent(0, false);
        frame(drawTable);
        io.AddKeyEvent(ImGuiKey_F2, true);
        frame(drawTable);
        io.AddKeyEvent(ImGuiKey_F2, false);
        frame(drawTable);
        io.AddInputCharactersUTF8("Edited");
        frame(drawTable);
        io.AddKeyEvent(ImGuiKey_Enter, true);
        frame(drawTable);
        io.AddKeyEvent(ImGuiKey_Enter, false);
        frame(drawTable);
        check(markdownTable.markdown().find("Edited") != std::string::npos,
              "Markdown Live table uses direct native grid cell editing");
        markdownTable.command(DocumentCommand::undo);
        frame(drawTable);
        check(markdownTable.markdown() == tableSource,
              "Markdown table edits use the source transaction history");
        auto pages = layoutRichDocument(document);
        check(pages.size() >= 2, "native layout honors explicit page breaks");
        RichBlock longBlock;
        longBlock.runs = {{DocumentInlineKind::text, std::string(8000, 'A')}};
        RichDocumentData longDocument;
        longDocument.blocks.push_back(longBlock);
        auto longPages = layoutRichDocument(longDocument);
        std::size_t characters = 0;
        for (const auto &p : longPages)
            for (const auto &fragment : p.fragments)
                characters += fragment.last - fragment.first;
        check(characters == 8000 && longPages.size() > 1, "long paragraph page fragments preserve all text");
        RichDocumentEditor editor;
        editor.setDocument(document);
        frame([&] { editor.draw("writer", {950, 700}); });
        check(editor.editBlock(0), "host can open a native paragraph editor");
        auto drawWriter = [&] { editor.draw("writer", {950, 700}); };
        frame(drawWriter);
        frame(drawWriter);
        ImGuiWindow *inputWindow = nullptr, *modal = nullptr;
        for (auto *window : GImGui->Windows)
            if (window->Active) {
                if (std::string(window->Name).find("###richEdit") != std::string::npos &&
                    !(window->Flags & ImGuiWindowFlags_ChildWindow))
                    modal = window;
                if (std::string(window->Name).find("##rich") != std::string::npos &&
                    (window->Flags & ImGuiWindowFlags_ChildWindow))
                    inputWindow = window;
            }
        check(inputWindow && modal, "paged paragraph editing uses a native rich input modal");
        if (inputWindow && modal) {
            auto cursor = inputWindow->InnerRect.Min;
            cursor.x += 10;
            cursor.y += 10;
            io.AddMousePosEvent(cursor.x, cursor.y);
            io.AddMouseButtonEvent(0, true);
            frame(drawWriter);
            io.AddMouseButtonEvent(0, false);
            frame(drawWriter);
            io.AddKeyEvent(ImGuiMod_Ctrl, true);
            io.AddKeyEvent(ImGuiKey_A, true);
            frame(drawWriter);
            io.AddKeyEvent(ImGuiKey_A, false);
            io.AddKeyEvent(ImGuiMod_Ctrl, false);
            frame(drawWriter);
            io.AddInputCharactersUTF8("Edited native heading");
            frame(drawWriter);
            auto apply = ImVec2(modal->Pos.x + 20, modal->DC.CursorPosPrevLine.y + 10);
            io.AddMousePosEvent(apply.x, apply.y);
            io.AddMouseButtonEvent(0, true);
            frame(drawWriter);
            io.AddMouseButtonEvent(0, false);
            frame(drawWriter);
            check(plain(editor.document()).find("Edited native heading") != std::string::npos,
                  "native paragraph editing commits to the rich document");
            editor.undo();
            check(plain(editor.document()) == plain(document), "paged paragraph changes share document undo");
        }
        SpreadsheetState spreadsheetState;
        frame([&] { spreadsheet("sheet", spreadsheetState, book, {950, 700}); });
        ImGui::DestroyContext();
        std::printf("Office checks: %s\n", failures ? "FAILED" : "passed");
        return failures ? 1 : 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "EXCEPTION: %s\n", e.what());
        return 2;
    }
}
