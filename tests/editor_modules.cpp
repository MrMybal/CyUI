#include <cyui/CyUI.h>
#include <cyui/detail/Json.h>
#include <imgui_internal.h>
#include <cstdio>
#include <limits>
#include <functional>
namespace {
int failures = 0;
void check(bool value, const char *message) {
    if (!value) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
}
template <class F> void frame(F draw) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({1100, 850});
    ImGui::Begin("Tests", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove);
    draw();
    ImGui::End();
    ImGui::Render();
}
} // namespace
int main() {
    using namespace cyui;
    check(evaluateNumericExpression("Value*100", 2.56).valid &&
              std::abs(evaluateNumericExpression("Value*100", 2.56).value - 256) < 1e-9,
          "Value expression uses original value");
    check(evaluateNumericExpression("-2^2 + 2^3^2", 0).value == 508,
          "precedence, unary and right associative powers");
    check(evaluateNumericExpression("clamp(max(Value, 2)*3, 0, 10)", 4).value == 10,
          "bounded function arguments");
    check(evaluateNumericExpression("1.5k+2e2", 0).value == 1700, "exponents and k suffix");
    for (auto text :
         {"1/0", "1%0", "sqrt(-1)", "Value*1e9999", "2 junk", "", "sin(1)", "1;system()", "min(1)", "1e+"})
        check(!evaluateNumericExpression(text, 2).valid, "invalid arithmetic is rejected");
    check(!evaluateNumericExpression(std::string(100, '(') + "1" + std::string(100, ')'), 0).valid,
          "expression depth is bounded");
    for (auto text : {"{\"a\":1,\"a\":2}", "[1,]", "01", "\"\\ud800\"", "1 true"}) {
        bool rejected = false;
        try {
            detail::jsonParse(text);
        } catch (const std::exception &) {
            rejected = true;
        }
        check(rejected, "strict JSON rejects malformed input");
    }
    auto exact = detail::jsonParse("{\"id\":18446744073709551615,\"name\":\"\\ud83d\\ude80\"}");
    check(exact.get("id").integer<std::uint64_t>() == UINT64_MAX &&
              exact.get("name").str() == "\xf0\x9f\x9a\x80",
          "JSON keeps uint64 and unicode surrogate pairs");
#if defined(CYUI_HAS_INSPECTOR)
    Property p;
    p.id = 1;
    p.key = "strength";
    p.label = "Strength";
    p.category = "Material";
    p.value = 2.56;
    p.bounded = true;
    p.minimum = 0;
    p.maximum = 10;
    PropertyValue value;
    std::string error;
    check(evaluatePropertyExpression(p, "Value*100", value, error) && std::get<double>(value) == 10,
          "requested 2.56 * 100 clamps immediately to 10");
    check(evaluatePropertyExpression(p, "(Value+1)/2", value, error) &&
              std::abs(std::get<double>(value) - 1.78) < 1e-12,
          "parenthesized arithmetic");
    check(!evaluatePropertyExpression(p, "Value/0", value, error) && !error.empty(),
          "invalid expression cannot commit");
    auto copied = p;
    copied.value = 256.;
    auto paste = propertyFromJson(p, propertyToJson(copied));
    check(bool(paste) && std::get<double>(paste.edits[0].edit.value) == 10,
          "JSON paste applies numeric bounds");
    auto other = p;
    other.value = std::string("same label, different type");
    check(!propertyFromJson(other, propertyToJson(p)), "clipboard rejects incompatible type");
    auto integer = p;
    integer.value = INT64_MAX;
    integer.bounded = false;
    auto integerPaste = propertyFromJson(integer, propertyToJson(integer));
    check(bool(integerPaste) && std::get<std::int64_t>(integerPaste.edits[0].edit.value) == INT64_MAX,
          "int64 JSON round trip preserves every bit");
    auto vector = p;
    vector.value = PropertyVector{{1, 2, 3}};
    vector.bounded = false;
    check(bool(propertyFromJson(vector, propertyToJson(vector))), "vector3 round trip");
    auto vector2 = vector;
    vector2.value = PropertyVector{{1, 2}};
    check(!propertyFromJson(vector2, propertyToJson(vector)), "vector shape mismatch rejected");
    auto asset = p;
    asset.value = PropertyAsset{"Texture", "/Game/Terrain"};
    asset.bounded = false;
    auto mesh = asset;
    mesh.value = PropertyAsset{"Mesh", "/Game/Mesh"};
    check(!propertyFromJson(mesh, propertyToJson(asset)), "asset class mismatch rejected");
    auto color = p;
    color.value = PropertyColor{{.2f, .3f, .4f, 1.f}};
    check(bool(propertyFromJson(color, propertyToJson(color))), "color round trip");
    auto list = p;
    list.value = PropertyArray{{std::int64_t(3), std::int64_t(4)}};
    list.bounded = false;
    check(bool(propertyFromJson(list, propertyToJson(list))), "collection round trip");
    std::vector<Property> fields{p, other};
    fields[1].key = "name";
    auto category = propertyCategoryToJson(fields, "Material");
    auto destination = fields;
    for (auto &f : destination) {
        f.category = "Other";
        f.id += 100;
    }
    auto categoryPaste = propertyCategoryFromJson(destination, "Other", category);
    check(bool(categoryPaste) && categoryPaste.edits.size() == 2 && categoryPaste.edits[0].id == 101,
          "whole category pastes by keys into another object/category");
    destination[1].readOnly = true;
    check(!propertyCategoryFromJson(destination, "Other", category),
          "read-only category field rejects entire transaction");
    check(!propertyFromJson(p, propertyToJson(p),
                            [](ItemId, const PropertyValue &) { return std::string("Host refuses"); }),
          "host validator runs for clipboard edits");
    auto selection = fields;
    selection[0].value = 4.;
    auto common = commonProperties({fields, selection});
    check(common.size() == 2 && common[0].mixed && !common[1].mixed,
          "multi-object inspector exposes mixed values");
    selection[0].maximum = 3;
    common = commonProperties({fields, selection});
    check(evaluatePropertyExpression(common[0], "Value*100", value, error) && std::get<double>(value) == 3,
          "multi-object edits use the intersection of all bounds");
#endif
#if defined(CYUI_HAS_HIERARCHY)
    std::vector<HierarchyItem> tree{{1, 0, "Root", "Scene"},
                                    {2, 1, "Camera", "Object"},
                                    {3, 1, "Light", "Object"},
                                    {4, 2, "Lens", "Object"},
                                    {5, 99, "Orphan", "Object"}};
    HierarchyState hierarchyState;
    check(hierarchyRows(tree, hierarchyState).size() == 5, "hierarchy includes orphan roots");
    hierarchyState.collapsed.insert(1);
    check(hierarchyRows(tree, hierarchyState).size() == 2, "collapsed descendants stay hidden");
    hierarchyState.query = "lens";
    auto filtered = hierarchyRows(tree, hierarchyState);
    check(filtered.size() == 3 && tree[filtered[2].index].id == 4,
          "filter retains ancestors and reveals descendants");
    std::vector<ItemId> moved{1};
    check(!canReparentHierarchy(tree, moved, 4), "reparent rejects descendant cycles");
    moved = {4};
    check(canReparentHierarchy(tree, moved, 3), "valid reparent allowed");
    tree[3].locked = true;
    check(!canReparentHierarchy(tree, moved, 0), "locked item cannot be reparented");
    std::vector<HierarchyItem> cycle{{1, 2, "A"}, {2, 1, "B"}};
    hierarchyState = {};
    check(hierarchyRows(cycle, hierarchyState).size() == 2, "cyclic input cannot hang hierarchy");
#endif
#if defined(CYUI_HAS_DATAVIEW)
    std::vector<DataColumn> columns{{1, "Name", true}, {2, "Value", true, true, 0, 10}, {3, "Exact", true}};
    std::vector<DataRow> rows{{1, {std::string("A\tB\n\"C\""), 2.56, INT64_MAX}},
                              {2, {std::string("B"), 5., std::int64_t(7)}},
                              {3, {std::string("C"), 1., std::int64_t(8)}}};
    DataViewState dataState;
    dataState.sort = {{1, false}};
    check(rows[dataViewRows(rows, dataState)[0]].id == 3, "numeric sort uses numeric order");
    dataState.filters = {"B"};
    check(dataViewRows(rows, dataState).size() == 2, "column filter applied");
    dataState = {};
    dataState.anchor = dataState.active = 1;
    dataState.anchorColumn = 0;
    dataState.activeColumn = 2;
    auto tsv = copyDataCells(rows, dataState);
    auto cells = pasteDataCells(columns, rows, dataState, tsv);
    check(bool(cells) && cells.edits.size() == 3 &&
              std::get<std::string>(cells.edits[0].edit.value) == std::get<std::string>(rows[0].cells[0]) &&
              std::get<std::int64_t>(cells.edits[2].edit.value) == INT64_MAX,
          "TSV quotes and exact integers round trip");
    dataState.columnOrder = {1, 0};
    dataState.anchorColumn = 1;
    dataState.activeColumn = 0;
    cells = pasteDataCells(columns, rows, dataState, copyDataCells(rows, dataState));
    check(bool(cells) && cells.edits.size() == 2 && cells.edits[0].column == 2 && cells.edits[1].column == 1,
          "clipboard follows visible column order after reordering/hiding");
    dataState.columnOrder.clear();
    dataState.anchorColumn = dataState.activeColumn = 1;
    cells = pasteDataCells(columns, rows, dataState, "Value*100");
    check(bool(cells) && std::get<double>(cells.edits[0].edit.value) == 10,
          "table numeric edits share expressions and bounds");
    cells = pasteDataCells(columns, rows, dataState, "3\nwrong");
    check(!cells && cells.edits.empty(), "bad multi-cell paste is atomic");
    rows[1].readOnly = true;
    check(!pasteDataCells(columns, rows, dataState, "3\n4"), "row read-only is enforced on paste");
#endif
#if defined(CYUI_HAS_FILEPICKER)
    check(normalizeFilePath("/project/text/../images", "/project") == "/project/images",
          "virtual paths normalize");
    check(normalizeFilePath("/project/../../outside", "/project").empty() &&
              normalizeFilePath("/project2", "/project").empty(),
          "root cannot be escaped or prefix-confused");
    FileListing listing{"/project", {{1, "A.PNG", false}, {2, "folder", true}}};
    FilePickerOptions pickerOptions;
    pickerOptions.root = "/project";
    pickerOptions.filters = {{"Images", {"png"}}};
    FilePickerState pickerState;
    pickerState.directory = "/project";
    pickerState.selection = {"A.PNG"};
    check(validateFilePicker(pickerState, listing, pickerOptions).accepted,
          "open accepts existing filtered file case insensitively");
    pickerState.selection = {"missing.png"};
    check(!validateFilePicker(pickerState, listing, pickerOptions).accepted, "open refuses nonexistent file");
    pickerOptions.mode = FilePickerMode::saveFile;
    pickerState.filename = "A.PNG";
    check(validateFilePicker(pickerState, listing, pickerOptions).overwriteRequested,
          "save requires overwrite confirmation");
    check(validateFilePicker(pickerState, listing, pickerOptions, true).accepted,
          "confirmed overwrite accepted");
    pickerState.filename = "../A.PNG";
    check(!validateFilePicker(pickerState, listing, pickerOptions, true).accepted,
          "save rejects path injection");
    listing.loading = true;
    pickerState.filename = "new.png";
    check(!validateFilePicker(pickerState, listing, pickerOptions).accepted,
          "stale/loading listing cannot validate a save");
#endif
#if defined(CYUI_HAS_NODES)
    std::vector<Node> nodes{{10, {0, 0}, "Source", {}, {{11, "Out"}}},
                            {20, {200, 0}, "Target", {{21, "In"}}, {}}};
    std::vector<NodeLink> links{{30, 11, 21}};
    std::vector<ItemId> nodeSelection{10, 20};
    std::vector<NodeGroup> groups{{40, {-10, -10}, {500, 300}, "Comment"}};
    std::vector<NodeReroute> routes{{50, 30, {150, 40}}};
    auto fragment = copyNodeSelection(nodeSelection, nodes, links, groups, routes);
    check(fragment.nodes.size() == 2 && fragment.links.size() == 1 && fragment.reroutes.size() == 1,
          "copy selection includes internal links and reroutes");
    NodeFragment restored;
    std::string nodeError;
    check(nodesFromJson(nodesToJson(fragment), restored, nodeError), "nodes JSON round trip");
    ItemId next = 100;
    check(cloneNodes(
              restored, [&] { return ++next; }, {20, 30}, restored, nodeError) &&
              restored.nodes[0].position.x == 20 &&
              restored.links[0].from == restored.nodes[0].outputs[0].id &&
              restored.reroutes[0].link == restored.links[0].id,
          "clone remaps pin/link/reroute IDs and offsets positions");
    check(!cloneNodes(
              fragment, [] { return ItemId(1); }, {}, restored, nodeError),
          "duplicate allocator IDs rejected");
    nodeSelection = {10};
    check(copyNodeSelection(nodeSelection, nodes, links).links.empty(), "external links are omitted");
#endif
#if defined(CYUI_HAS_TIMELINE)
    std::vector<TimelineClip> clips{{1, 1, 1, 4, "Clip"}, {2, 1, 6, 2, "Locked", 0, true}};
    std::vector<ItemId> clipSelection{1, 2};
    ItemId clipId = 10;
    auto allocateClip = [&] { return ++clipId; };
    auto split = splitTimelineClips(clips, clipSelection, 3, allocateClip);
    check(split.changed && split.clips.size() == 3 && split.clips[0].duration == 2 &&
              split.clips[1].duration == 2,
          "split preserves extent and skips locked clips");
    auto copies = duplicateTimelineClips(clips, clipSelection, 5, allocateClip);
    check(copies.changed && copies.clips.size() == 3 && copies.clips[1].start == 6,
          "duplicate offsets editable clips only");
    check(!duplicateTimelineClips(clips, clipSelection, 5, [] { return ItemId(1); }).error.empty(),
          "timeline rejects existing clone ID");
    std::vector<TimelineLane> lanes{{1, "A"}, {2, "B"}, {3, "C"}};
    check(reorderTimelineLane(lanes, 3, 1).front().id == 3, "lane reorder preserves IDs");
#endif
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1100, 850};
    io.DeltaTime = 1.f / 60;
    io.ConfigInputTrickleEventQueue = false;
    apply();
    unsigned char *pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID(ImTextureID(1));
#if defined(CYUI_HAS_DATAVIEW)
    {
        std::vector<DataRow> tableRows;
        for (int n = 0; n < 500; ++n)
            tableRows.push_back({ItemId(n + 1), {std::string("Row ") + std::to_string(n)}});
        std::vector<DataColumn> tableColumns{{1, "Name", true}};
        DataViewState tableState;
        for (auto density : {Density::compact, Density::comfortable}) {
            setDensity(density);
            auto draw = [&] { dataView("rowPitch", tableState, tableColumns, tableRows, {600, 300}); };
            frame(draw);
            frame(draw);
            frame(draw);
            ImGuiWindow *tableWindow = nullptr;
            for (auto *window : ImGui::GetCurrentContext()->Windows)
                if (window->LastFrameActive == ImGui::GetFrameCount() &&
                    (window->Flags & ImGuiWindowFlags_ChildWindow))
                    tableWindow = window;
            const float pitch = std::max(ImGui::GetFrameHeightWithSpacing(),
                                         ImGui::GetFrameHeight() + ImGui::GetStyle().CellPadding.y * 2);
            check(tableWindow && std::abs(tableWindow->ContentSize.y - pitch * 500) < pitch * 4,
                  "virtual table extent includes actual cell padding at both densities");
        }
        setDensity(Density::compact);
    }
#endif
#if defined(CYUI_HAS_COMMANDS)
    CommandRegistry registry;
    check(registry.add({1, "Save", "Ctrl+S", "File"}), "register global command");
    Command contextual{2, "Save selection", "Ctrl+S", "Editor"};
    contextual.context = "editor";
    check(registry.add(contextual), "different contexts can reuse chord");
    check(registry.match(parseShortcut("Ctrl+S"), {"editor"}) == 2 &&
              registry.match(parseShortcut("Ctrl+S")) == 1,
          "active context wins over global");
    check(!registry.match(parseShortcut("Ctrl+S"), {"editor"}, true), "typing protects editor shortcuts");
    check(!registry.add({3, "Conflict", "Ctrl+S"}), "same-context shortcut collision refused");
    check(registry.add({3, "Other", "Ctrl+O"}) && !registry.rebind(3, parseShortcut("Ctrl+S")),
          "rebinding conflicts refused");
    auto bindings = registry.bindingsJson();
    std::string bindingError;
    check(registry.rebind(1, parseShortcut("Alt+S")) && registry.loadBindingsJson(bindings, bindingError) &&
              registry.find(1)->shortcut == "Ctrl+S",
          "bindings JSON round trip");
    check(!parseShortcut("Ctrl+Banana") && !parseShortcut("Ctrl+Ctrl+S"), "invalid chords rejected");
#endif
    for (const auto &theme : builtInThemes()) {
        setTheme(theme.key.c_str());
        frame([&] {
#if defined(CYUI_HAS_INSPECTOR)
            static InspectorState state;
            Property v = p;
            v.category = "Material";
            std::vector<Property> advanced{v, vector, color, asset, list};
            inspector("inspector", state, advanced);
#endif
#if defined(CYUI_HAS_HIERARCHY)
            hierarchy("hierarchy", hierarchyState, tree, {300, 200});
#endif
#if defined(CYUI_HAS_DATAVIEW)
            dataView("data", dataState, columns, rows, {600, 200});
#endif
#if defined(CYUI_HAS_FILEPICKER)
            filePicker("files", pickerState, listing, {600, 400}, pickerOptions);
#endif
        });
    }
#if defined(CYUI_HAS_INSPECTOR)
    {
        Property field = p;
        field.id = 400;
        field.category.clear();
        field.value = 2.56;
        InspectorState state;
        std::vector<Property> uiFields{field};
        ImVec2 min, max;
        auto draw = [&] {
            for (const auto &event : inspector("expressionUi", state, uiFields))
                if (event.edit.committed())
                    uiFields[0].value = event.edit.value;
            min = ImGui::GetItemRectMin();
            max = ImGui::GetItemRectMax();
        };
        frame(draw);
        frame(draw);
        const ImVec2 mouse{(min.x + max.x) * .5f, (min.y + max.y) * .5f};
        io.AddMousePosEvent(mouse.x, mouse.y);
        io.AddMouseButtonEvent(0, true);
        frame(draw);
        io.AddMouseButtonEvent(0, false);
        frame(draw);
        io.AddMouseButtonEvent(0, true);
        frame(draw);
        io.AddMouseButtonEvent(0, false);
        frame(draw);
        frame(draw);
        check(state.drafts[400].expressionOpen, "double-click opens the numeric expression editor");
        io.AddInputCharactersUTF8("Value*100");
        frame(draw);
        io.AddKeyEvent(ImGuiKey_Enter, true);
        frame(draw);
        io.AddKeyEvent(ImGuiKey_Enter, false);
        frame(draw);
        check(std::get<double>(uiFields[0].value) == 10 && !state.drafts[400].expressionOpen,
              "typed Value*100 commits a clamped field value through real input events");
    }
#endif
    ImGui::DestroyContext();
    if (!failures)
        std::puts("Editor module models, transactions and UI frames passed");
    return failures ? 1 : 0;
}
