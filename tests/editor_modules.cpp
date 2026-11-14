#include <cyui/CyUI.h>
#include <cyui/detail/Json.h>
#include <imgui_internal.h>
#include <cstdio>
#include <limits>
#include <functional>
#if defined(CYUI_HAS_INSPECTOR)
#include <cyui/examples/EditorWorkspace.h>
#endif
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
    auto typedEmpty = list;
    typedEmpty.value = makePropertyArray(PropertyVector{{1, 2, 3}});
    auto emptyStrings = list;
    emptyStrings.value = makePropertyArray(std::string{});
    check(!propertyFromJson(typedEmpty, propertyToJson(emptyStrings)),
          "empty collections preserve their element schema");
    check(bool(propertyFromJson(typedEmpty, propertyToJson(typedEmpty))),
          "typed empty collection round trip");
    auto largeList = list;
    largeList.value = makePropertyArray(2.56, 10000);
    check(bool(propertyFromJson(largeList, propertyToJson(largeList))),
          "JSON collections are no longer capped at 1024 elements");
    PropertyObject record{"SpawnPoint",
                          {{"name", std::string("A")},
                           {"position", PropertyVector{{1, 2, 3}}},
                           {"weights", makePropertyArray(.5f, 3)}}};
    auto records = list;
    records.value = makePropertyArray(record, 2);
    check(bool(propertyFromJson(records, propertyToJson(records))),
          "arrays of records and nested arrays round trip");
    auto wrongRecords = records;
    std::get<PropertyObject>(std::get<PropertyArray>(wrongRecords.value).values[1]).fields["name"] = 42.;
    check(!propertyFromJson(records, propertyToJson(wrongRecords)),
          "nested record field types are validated atomically");
    auto fewerRecords = records;
    fewerRecords.value = makePropertyArray(record, 1);
    check(commonProperties({{records}, {fewerRecords}}).size() == 1,
          "record collection schema is independent of element count/values");
    auto wrongSchema = records;
    wrongSchema.value = makePropertyArray(PropertyObject{"SpawnPoint", {{"name", 42.}}}, 1);
    check(commonProperties({{records}, {wrongSchema}}).empty(),
          "multi-object collections require matching recursive schemas");
    auto custom = list;
    custom.value = makePropertyArray(PropertyCustom{"Angle", "45"}, 2);
    check(bool(propertyFromJson(custom, propertyToJson(custom))),
          "host-defined types use portable JSON payloads");
    auto wrongCustom = custom;
    wrongCustom.value = makePropertyArray(PropertyCustom{"Time", "45"}, 2);
    check(!propertyFromJson(custom, propertyToJson(wrongCustom)),
          "custom type identifiers prevent incompatible paste");
    auto invalidCustom = custom;
    auto customValue = PropertyValue{makePropertyArray(PropertyCustom{"Angle", "{bad json}"}, 2)};
    check(!normalizePropertyValue(invalidCustom, customValue, error), "custom payloads must be strict JSON");
    PropertyValue tooDeep = 1.;
    for (int n = 0; n < 14; ++n)
        tooDeep = PropertyArray{{std::move(tooDeep)}};
    auto deepProperty = list;
    deepProperty.value = tooDeep;
    check(!normalizePropertyValue(deepProperty, tooDeep, error),
          "recursive models have a bounded nesting depth");
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
    {
        Property unique;
        unique.id = 700;
        unique.label = "Set";
        auto setValue = makePropertySet(0.);
        setValue.values = {1., 2., 3.};
        unique.value = setValue;
        check(bool(propertyFromJson(unique, propertyToJson(unique))),
              "Set JSON preserves typed element schema");
        auto reordered = setValue;
        std::reverse(reordered.values.begin(), reordered.values.end());
        check(setValue == reordered, "Set identity is independent of presentation order");
        auto duplicate = PropertyValue{PropertySet{{0., -0.}, setValue.elementTemplate}};
        check(!normalizePropertyValue(unique, duplicate, error),
              "Set rejects equivalent signed-zero elements");
        unique.bounded = true;
        unique.minimum = 0;
        unique.maximum = 10;
        duplicate = PropertySet{{10., 256.}, setValue.elementTemplate};
        check(!normalizePropertyValue(unique, duplicate, error),
              "Set uniqueness is checked after numeric clamp");
        auto mapping = makePropertyMap(PropertyVector{{0, 0}}, PropertyArray{{std::string("A")}});
        mapping.entries = {{PropertyVector{{1, 2}}, PropertyArray{{std::string("A")}}},
                           {PropertyVector{{3, 4}}, PropertyArray{{std::string("B")}}}};
        Property mapped;
        mapped.id = 701;
        mapped.value = mapping;
        check(bool(propertyFromJson(mapped, propertyToJson(mapped))),
              "Map accepts vector keys and collection values");
        auto reverseMap = mapping;
        std::reverse(reverseMap.entries.begin(), reverseMap.entries.end());
        check(reverseMap == mapping, "Map identity is independent of entry order");
        auto repeatedMap = mapping;
        repeatedMap.entries[1].key = repeatedMap.entries[0].key;
        auto repeated = PropertyValue{repeatedMap};
        check(!normalizePropertyValue(mapped, repeated, error),
              "Map rejects duplicate typed keys atomically");
        auto badMap = mapped;
        badMap.value = makePropertyMap(std::string{}, PropertyArray{{std::string("A")}});
        check(!propertyFromJson(mapped, propertyToJson(badMap)),
              "Empty Map key templates remain type checked");
        check(commonProperties({{mapped}, {badMap}}).empty(), "Multi-object Map schemas must agree");
        Property enumeration;
        enumeration.id = 702;
        enumeration.value = PropertyEnum{"Quality", 1, {{0, "Low"}, {1, "High"}}};
        auto badEnum = PropertyValue{PropertyEnum{"Quality", 99, {{0, "Low"}, {1, "High"}}}};
        check(!normalizePropertyValue(enumeration, badEnum, error), "Enums reject unknown enumerators");
        Property flags;
        flags.id = 703;
        flags.value = PropertyEnum{"Channels", 3, {{0, "None"}, {1, "A"}, {2, "B"}, {4, "C"}}, true};
        check(bool(propertyFromJson(flags, propertyToJson(flags))), "Flags JSON round trip");
        auto badFlags = flags.value;
        std::get<PropertyEnum>(badFlags).value = 8;
        check(!normalizePropertyValue(flags, badFlags, error), "Flags reject undefined bits");
        Property optional;
        optional.id = 704;
        optional.value = makePropertyOptional(enumeration.value);
        check(bool(propertyFromJson(optional, propertyToJson(optional))),
              "Nullable enum retains its empty schema");
        auto present = optional;
        present.value = makePropertyOptional(enumeration.value, true);
        check(bool(propertyFromJson(optional, propertyToJson(present))), "Optional enables a typed value");
        auto detached = present.value;
        std::get<PropertyEnum>(*std::get<PropertyOptional>(detached).value).value = 0;
        check(std::get<PropertyEnum>(*std::get<PropertyOptional>(present.value).value).value == 1,
              "Optional copies own independent values");
        PropertyMetadata immutable;
        immutable.readOnly = true;
        PropertyMetadata gain;
        gain.bounded = true;
        gain.minimum = 0;
        gain.maximum = 10;
        PropertyMetadata choices;
        choices.choices = {"A", "B"};
        PropertyObject firstRecord{"Record",
                                   {{"id", std::int64_t(1)}, {"gain", 2.56}, {"name", std::string("A")}},
                                   {{"id", immutable}, {"gain", gain}, {"name", choices}}};
        auto secondRecord = firstRecord;
        secondRecord.fields["id"] = std::int64_t(2);
        secondRecord.fields["name"] = std::string("B");
        Property optionalRecord;
        optionalRecord.id = 708;
        auto optionalValue = makePropertyOptional(firstRecord, true);
        *optionalValue.value = secondRecord;
        optionalRecord.value = optionalValue;
        check(bool(propertyFromJson(optionalRecord, propertyToJson(optionalRecord))),
              "Optional records retain current readonly fields independently of Add defaults");
        auto invalidOptional = optionalRecord;
        std::get<PropertyObject>(*std::get<PropertyOptional>(invalidOptional.value).value).fields["id"] =
            std::int64_t(9);
        check(!propertyFromJson(optionalRecord, propertyToJson(invalidOptional)),
              "Optional records reject changes to current readonly fields");
        Property recordsWithRules;
        recordsWithRules.id = 705;
        recordsWithRules.value =
            PropertyArray{{firstRecord, secondRecord}, std::make_shared<const PropertyValue>(firstRecord)};
        check(bool(propertyFromJson(recordsWithRules, propertyToJson(recordsWithRules))),
              "Record field metadata is restored from the destination");
        auto movedRecords =
            editPropertyCollection(recordsWithRules, {0}, PropertyCollectionAction::move, {}, 2);
        check(bool(movedRecords) &&
                  std::get<std::int64_t>(
                      std::get<PropertyObject>(
                          std::get<PropertyArray>(movedRecords.edits[0].edit.value).values[0])
                          .fields.at("id")) == 2,
              "Readonly fields remain attached to records during array reordering");
        check(bool(editPropertyCollection(recordsWithRules, {0}, PropertyCollectionAction::remove)),
              "Removing a record does not rewrite another record's readonly fields");
        auto changedRecords = recordsWithRules.value;
        auto &changedFields =
            std::get<PropertyObject>(std::get<PropertyArray>(changedRecords).values[1]).fields;
        changedFields["gain"] = 256.;
        check(normalizePropertyValue(recordsWithRules, changedRecords, error) &&
                  std::get<double>(std::get<PropertyObject>(std::get<PropertyArray>(changedRecords).values[1])
                                       .fields.at("gain")) == 10.,
              "Per-field bounds clamp inside array records");
        changedFields["id"] = std::int64_t(9);
        check(!normalizePropertyValue(recordsWithRules, changedRecords, error),
              "Per-field readonly rejects unauthorized clipboard changes");
        Property batch;
        batch.id = 706;
        batch.value = PropertyArray{{2.56, 4., 8.}};
        batch.bounded = true;
        batch.maximum = 10;
        auto bulk = editPropertyCollection(batch, {0, 1}, PropertyCollectionAction::expression, "Value*100");
        check(bool(bulk) &&
                  std::get<double>(std::get<PropertyArray>(bulk.edits[0].edit.value).values[1]) == 10.,
              "Bulk expressions use each selected original value and clamp atomically");
        check(!editPropertyCollection(batch, {0, 1}, PropertyCollectionAction::expression, "Value/0"),
              "Invalid bulk arithmetic does not return partial edits");
        check(!editPropertyCollection(unique, {0, 1}, PropertyCollectionAction::expression, "Value*100"),
              "Bulk Set edits reject uniqueness collisions");
        check(!editPropertyCollection(
                  batch, {0}, PropertyCollectionAction::remove, {}, std::size_t(-1),
                  [](ItemId, const PropertyValue &) { return std::string("Host refuses"); }),
              "Bulk changes run the host validator once for the whole property");
    }
    {
        examples::EditorWorkspace workspace;
        auto original = workspace.find(1)->properties[4].value;
        PropertyValue calculated;
        std::string message;
        check(evaluatePropertyExpression(workspace.find(1)->properties[4], "Value*100", calculated, message),
              "Integrated app prepares an Inspector expression transaction");
        check(workspace.apply({{1, 5, calculated}, {2, 2, PropertyVector{{10, 20, 30}}}}),
              "Integrated app applies Inspector and DataView changes atomically");
        check(std::get<double>(workspace.find(1)->properties[4].value) == 10 &&
                  std::get<PropertyVector>(workspace.find(2)->properties[1].value).values[0] == 10,
              "Integrated app views share one updated model");
        check(workspace.undo() && workspace.find(1)->properties[4].value == original &&
                  std::get<PropertyVector>(workspace.find(2)->properties[1].value).values[0] == 150,
              "One Undo restores all fields in the shared transaction");
        check(workspace.redo() && std::get<double>(workspace.find(1)->properties[4].value) == 10,
              "Integrated app Redo restores the full transaction");
        const auto revision = workspace.revision();
        check(!workspace.apply({{1, 5, 3.}, {2, 2, std::string("Wrong type")}}) &&
                  workspace.revision() == revision &&
                  std::get<double>(workspace.find(1)->properties[4].value) == 10,
              "An invalid shared transaction changes neither model nor history");
        check(workspace.apply({{1, 4, PropertyAsset{"Texture", "/Game/Textures/Stone.png"}}}) &&
                  workspace.undo(),
              "Host asset selection participates in the same Undo history");
        check(workspace.apply({{1, 5, 4.}}) && !workspace.canRedo(),
              "A new edit after Undo discards the obsolete Redo branch");
    }
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
    {
        HierarchyState navigation;
        navigation.selection = {1};
        navigation.focused = 1;
        navigation.collapsed.insert(1);
        navigateHierarchy(tree, navigation, HierarchyNavigation::right);
        check(!navigation.collapsed.count(1), "Right expands a keyboard-focused hierarchy branch");
        navigateHierarchy(tree, navigation, HierarchyNavigation::right);
        check(navigation.selection == std::vector<ItemId>{2}, "Right enters the first child");
        navigateHierarchy(tree, navigation, HierarchyNavigation::down, true);
        check(navigation.selection == std::vector<ItemId>({2, 4}),
              "Shift-down extends visible hierarchy selection");
        navigateHierarchy(tree, navigation, HierarchyNavigation::left);
        check(navigation.focused == 2, "Left navigates from a leaf to its visible parent");
        navigateHierarchy(tree, navigation, HierarchyNavigation::end);
        check(navigation.selection == std::vector<ItemId>{5}, "End selects the final visible root");
    }
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
    {
        std::vector<DataCell> richValues{PropertyVector{{1, 2, 3}},
                                         PropertyColor{},
                                         PropertyAsset{"Texture", "/Game/Test"},
                                         PropertyCustom{"Angle", "45"},
                                         makePropertyArray(2.56, 2),
                                         PropertySet{{std::string("A"), std::string("B")},
                                                     std::make_shared<const PropertyValue>(std::string{})},
                                         PropertyMap{{{std::string("A"), 2.56}},
                                                     std::make_shared<const PropertyValue>(std::string{}),
                                                     std::make_shared<const PropertyValue>(0.)},
                                         makePropertyOptional(PropertyVector{{0, 0, 0}}),
                                         PropertyEnum{"Quality", 1, {{0, "Low"}, {1, "High"}}}};
        std::vector<DataColumn> richColumns;
        for (std::size_t c = 0; c < richValues.size(); ++c)
            richColumns.push_back({ItemId(c + 1), "Value", true});
        std::vector<DataRow> richRows{{1, richValues}};
        DataViewState selectedCells;
        selectedCells.anchor = selectedCells.active = 1;
        selectedCells.anchorColumn = 0;
        selectedCells.activeColumn = richValues.size() - 1;
        auto richPaste =
            pasteDataCells(richColumns, richRows, selectedCells, copyDataCells(richRows, selectedCells));
        check(bool(richPaste) && richPaste.edits.size() == richValues.size(),
              "Every rich DataView cell round trips through typed quoted TSV");
        if (richPaste)
            for (std::size_t c = 0; c < richValues.size(); ++c)
                check(richPaste.edits[c].edit.value == richValues[c],
                      "Rich DataView paste preserves typed values");
        richColumns.back().metadata.readOnly = true;
        check(!pasteDataCells(richColumns, richRows, selectedCells, copyDataCells(richRows, selectedCells)),
              "Readonly column metadata rejects an entire rich cell paste");
    }
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
    {
        auto nestedGroups = groups;
        nestedGroups.push_back({41, {0, 0}, {250, 100}, "Child"});
        nestedGroups.push_back({42, {450, 0}, {250, 100}, "Overlapping, outside"});
        NodeSelection selected;
        selected.groups = {40, 41};
        selected.nodes = {10};
        auto copied = copyGraphSelection(selected, nodes, links, nestedGroups, routes);
        check(copied.groups.size() == 2 && copied.nodes.size() == 2 && copied.links.size() == 1 &&
                  copied.reroutes.size() == 1,
              "Typed group copy closes over nested groups and content without duplicates or overlapping "
              "groups");
        selected = {};
        selected.links = {30};
        copied = copyGraphSelection(selected, nodes, links, nestedGroups, routes);
        check(copied.nodes.size() == 2 && copied.groups.empty() && copied.reroutes.size() == 1,
              "Copying a link includes its endpoint nodes, pins and routing");
        selected = {};
        selected.reroutes = {50};
        copied = copyGraphSelection(selected, nodes, links, nestedGroups, routes);
        check(copied.nodes.size() == 2 && copied.links.size() == 1 && copied.reroutes.size() == 1,
              "Copying a reroute produces a self-contained cross-graph fragment");
        auto overlappingIds = nestedGroups;
        overlappingIds[0].id = 10;
        selected = {};
        selected.nodes = {10};
        check(copyGraphSelection(selected, nodes, links, overlappingIds, routes).groups.empty(),
              "Typed selection preserves distinct ID namespaces");
        auto invalid = nodesToJson(copied);
        const auto at = invalid.find("\"id\":10");
        check(at != std::string::npos, "Clipboard fixture contains the source node");
        if (at != std::string::npos)
            invalid.replace(at, 7, "\"id\":0");
        const auto previous = restored.nodes[0].id;
        check(!nodesFromJson(invalid, restored, nodeError) && restored.nodes[0].id == previous,
              "Invalid zero IDs reject the whole clipboard without replacing output");
    }
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
#if defined(CYUI_HAS_NODES)
    {
        static std::string clipboard;
        auto &platform = ImGui::GetPlatformIO();
        const auto get = platform.Platform_GetClipboardTextFn;
        const auto set = platform.Platform_SetClipboardTextFn;
        platform.Platform_GetClipboardTextFn = [](ImGuiContext *) { return clipboard.c_str(); };
        platform.Platform_SetClipboardTextFn = [](ImGuiContext *, const char *text) { clipboard = text; };
        std::vector<Node> model{{1, {80, 80}, "A", {}, {{11, "Out"}}}, {2, {400, 80}, "B", {{21, "In"}}, {}}};
        std::vector<NodeLink> connections{{3, 11, 21}};
        std::vector<NodeGroup> boxes{{10, {20, 20}, {650, 220}, "Parent"},
                                     {12, {50, 50}, {550, 170}, "Child"}};
        std::vector<NodeReroute> points{{20, 3, {310, 150}}};
        NodeState state;
        NodeResult result;
        NodeOptions options;
        options.groups = boxes;
        options.reroutes = points;
        ItemId nextId = 100;
        options.allocateId = [&] { return ++nextId; };
        ImVec2 origin;
        auto draw = [&] {
            origin = ImGui::GetCursorScreenPos();
            result = nodeEditor("clipboardGraph", state, model, connections, {1000, 350}, options);
        };
        auto shortcut = [&](ImGuiKey key) {
            io.AddKeyEvent(ImGuiMod_Ctrl, true);
            frame(draw);
            io.AddKeyEvent(key, true);
            frame(draw);
        };
        auto release = [&](ImGuiKey key) {
            io.AddKeyEvent(key, false);
            frame(draw);
            io.AddKeyEvent(ImGuiMod_Ctrl, false);
            frame(draw);
        };
        frame(draw);
        frame(draw);
        io.AddMousePosEvent(origin.x + 30, origin.y + 30);
        io.AddMouseButtonEvent(1, true);
        frame(draw);
        io.AddMouseButtonEvent(1, false);
        frame(draw);
        frame(draw);
        check(state.selectedGroups == std::vector<ItemId>{10},
              "Group context menu selects the clicked group");
        auto *popup = GImGui->OpenPopupStack.empty() ? nullptr : GImGui->OpenPopupStack.back().Window;
        check(popup != nullptr, "Node clipboard context menu opens");
        if (popup) {
            io.AddMousePosEvent(popup->Pos.x + popup->WindowPadding.x + dp(30),
                                popup->Pos.y + popup->WindowPadding.y + ImGui::GetFrameHeight() * .5f);
            io.AddMouseButtonEvent(0, true);
            frame(draw);
            io.AddMouseButtonEvent(0, false);
            frame(draw);
            frame(draw);
            NodeFragment copied;
            std::string error;
            check(nodesFromJson(clipboard, copied, error) && copied.nodes.size() == 2 &&
                      copied.groups.size() == 2 && copied.links.size() == 1 && copied.reroutes.size() == 1,
                  "Context Copy exports nested groups, nodes, pins, links and reroutes through real input");
            io.AddMousePosEvent(origin.x + 760, origin.y + 300);
            frame(draw);
            shortcut(ImGuiKey_V);
            check(result.created.groups.size() == 2 && result.created.nodes.size() == 2 &&
                      result.created.links.size() == 1 && result.created.reroutes.size() == 1 &&
                      result.created.groups[0].position.x == 760,
                  "Ctrl+V remaps the complete graph and pastes its anchor at the pointer");
            release(ImGuiKey_V);
            clipboard = "{invalid";
            const auto kept = state.selection;
            shortcut(ImGuiKey_V);
            check(result.created.nodes.empty() && state.selection == kept && !state.clipboardError.empty(),
                  "Malformed paste is atomic and preserves selection");
            release(ImGuiKey_V);
            options.readOnly = true;
            shortcut(ImGuiKey_V);
            check(result.created.nodes.empty(), "Read-only graphs block paste");
            release(ImGuiKey_V);
        }
        platform.Platform_GetClipboardTextFn = get;
        platform.Platform_SetClipboardTextFn = set;
    }
    {
        std::vector<Node> movingNodes{{1, {50, 80}, "Inside"}, {2, {600, 80}, "Outside"}};
        std::vector<NodeGroup> movingGroups{{10, {20, 20}, {400, 240}, "Group"}};
        std::vector<NodeReroute> movingRoutes{{20, 1, {180, 100}}, {21, 1, {600, 100}}};
        NodeState movingState;
        NodeOptions movingOptions;
        movingOptions.groups = movingGroups;
        movingOptions.reroutes = movingRoutes;
        NodeResult movedGroup;
        ImVec2 origin;
        auto drawGroup = [&] {
            origin = ImGui::GetCursorScreenPos();
            movedGroup =
                nodeEditor("groupContents", movingState, movingNodes, {}, {1000, 330}, movingOptions);
        };
        frame(drawGroup);
        frame(drawGroup);
        io.AddMousePosEvent(origin.x + 30, origin.y + 30);
        io.AddMouseButtonEvent(0, true);
        frame(drawGroup);
        check(movedGroup.moved.phase == EditPhase::begin,
              "Moving a comment group begins a content transaction");
        io.AddMousePosEvent(origin.x + 80, origin.y + 55);
        frame(drawGroup);
        io.AddMouseButtonEvent(0, false);
        frame(drawGroup);
        check(movedGroup.moved.committed() && movedGroup.moved.value.size() == 1 &&
                  movedGroup.moved.value[0].id == 1 && movedGroup.moved.value[0].position.x == 100 &&
                  movedGroup.moved.value[0].position.y == 105,
              "Group drag commits contained nodes exactly once and leaves outside nodes unchanged");
        check(movedGroup.reroutes.committed() && movedGroup.reroutes.value[0].position.x == 230 &&
                  movedGroup.reroutes.value[1].position.x == 600,
              "Group drag moves contained reroutes and keeps external reroutes fixed");
        check(movingNodes[0].position.x == 50, "Group gesture proposals leave the host model untouched");
        io.AddMousePosEvent(origin.x + 30, origin.y + 30);
        io.AddMouseButtonEvent(0, true);
        frame(drawGroup);
        io.AddKeyEvent(ImGuiKey_Escape, true);
        frame(drawGroup);
        check(movedGroup.groups.phase == EditPhase::cancel && movedGroup.moved.phase == EditPhase::cancel &&
                  movedGroup.reroutes.phase == EditPhase::cancel,
              "Escape cancels group, node and route gestures together");
        io.AddKeyEvent(ImGuiKey_Escape, false);
        io.AddMouseButtonEvent(0, false);
        frame(drawGroup);
    }
#endif
#if defined(CYUI_HAS_DATAVIEW)
    {
        std::vector<DataColumn> richColumns{{8, "Angle", true}};
        std::vector<DataRow> richRows{{11, {PropertyCustom{"Angle", "45"}}}};
        DataViewState editingCell;
        editingCell.editing = 11;
        editingCell.editingColumn = 0;
        DataViewOptions editingOptions;
        ImVec2 buttonPosition;
        editingOptions.editors.editCustom = [&](ItemId owner, const std::vector<std::string> &path,
                                                const PropertyCustom &custom) {
            check(owner == 8 && path == std::vector<std::string>{"11"},
                  "DataView custom editors receive column ID and row path");
            const bool clicked = ImGui::Button(custom.json.c_str());
            const auto lo = ImGui::GetItemRectMin(), hi = ImGui::GetItemRectMax();
            buttonPosition = {(lo.x + hi.x) * .5f, (lo.y + hi.y) * .5f};
            return clicked ? Edit<PropertyCustom>{EditPhase::commit, {custom.type, "60"}}
                           : Edit<PropertyCustom>{};
        };
        auto drawGrid = [&] {
            for (const auto &event :
                 dataView("richCellCommit", editingCell, richColumns, richRows, {600, 200}, editingOptions))
                if (event.edit.committed())
                    richRows[0].cells[0] = event.edit.value;
        };
        frame(drawGrid);
        frame(drawGrid);
        io.AddMousePosEvent(buttonPosition.x, buttonPosition.y);
        io.AddMouseButtonEvent(0, true);
        frame(drawGrid);
        io.AddMouseButtonEvent(0, false);
        frame(drawGrid);
        check(std::get<PropertyCustom>(richRows[0].cells[0]).json == "60",
              "DataView commits typed custom values through the shared property editor");
    }
#endif
#if defined(CYUI_HAS_INSPECTOR)
    for (const auto &theme : builtInThemes()) {
        setTheme(theme.key.c_str());
        for (auto density : {Density::compact, Density::comfortable}) {
            setDensity(density);
            auto map = makePropertyMap(std::string{}, PropertyVector{{0, 0, 0}});
            map.entries = {{std::string("A"), PropertyVector{{1, 2, 3}}},
                           {std::string("B"), PropertyVector{{4, 5, 6}}}};
            std::vector<PropertyValue> extraTypes{
                PropertySet{{std::int64_t(1), std::int64_t(2)},
                            std::make_shared<const PropertyValue>(std::int64_t(0))},
                map,
                makePropertyOptional(2.56),
                makePropertyOptional(PropertyVector{{1, 2, 3}}, true),
                PropertyEnum{"Quality", 1, {{0, "Low"}, {1, "High"}}},
                PropertyEnum{"Flags", 3, {{1, "A"}, {2, "B"}}, true}};
            for (const auto &type : extraTypes) {
                Property field;
                field.id = 800;
                field.label = "Extended type";
                field.value = type;
                std::vector<Property> model{field};
                InspectorState state;
                frame([&] { inspector("extendedTypes", state, model); });
                frame([&] { inspector("extendedTypes", state, model); });
            }
        }
    }
    setDensity(Density::compact);
    {
        Property field;
        field.id = 500;
        field.label = "Large collection";
        field.value = makePropertyArray(PropertyCustom{"Counter", "0"}, 100000);
        std::vector<Property> model{std::move(field)};
        InspectorState state;
        InspectorOptions options;
        options.arrayHeight = 150;
        ImGuiWindow *listWindow = nullptr;
        ImVec2 firstButton;
        std::vector<std::string> rendered;
        options.editCustom = [&](ItemId owner, const std::vector<std::string> &path,
                                 const PropertyCustom &v) {
            check(owner == 500 && path.size() == 1, "custom editor receives owner and element path");
            rendered.push_back(path.front());
            listWindow = ImGui::GetCurrentWindow();
            const bool clicked = ImGui::Button((v.json + "##counter").c_str());
            if (path.front() == "0") {
                const auto min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
                firstButton = {(min.x + max.x) * .5f, (min.y + max.y) * .5f};
            }
            return clicked ? Edit<PropertyCustom>{EditPhase::commit, {v.type, "1"}} : Edit<PropertyCustom>{};
        };
        auto draw = [&] {
            rendered.clear();
            for (const auto &event : inspector("large", state, model, 0, {}, options))
                if (event.edit.committed())
                    model[0].value = event.edit.value;
        };
        frame(draw);
        frame(draw);
        check(!rendered.empty() && rendered.size() < 20,
              "100000 elements only submit visible collection rows");
        auto data = std::get<PropertyArray>(state.drafts[500].value).values.data();
        frame(draw);
        check(data == std::get<PropertyArray>(state.drafts[500].value).values.data(),
              "idle large collection drafts do not copy the model each frame");
        const float pitch = ImGui::GetFrameHeightWithSpacing();
        check(listWindow && std::abs(listWindow->ContentSize.y - pitch * 100000) < pitch * 3,
              "virtual collection scrollbar covers the full model");
        io.AddMousePosEvent(firstButton.x, firstButton.y);
        io.AddMouseButtonEvent(0, true);
        frame(draw);
        ImGui::SetScrollY(listWindow, listWindow->ScrollMax.y * .5f);
        frame(draw);
        frame(draw);
        check(std::find(rendered.begin(), rendered.end(), "0") != rendered.end() && rendered.size() < 22,
              "active collection row remains submitted when scrolled outside the viewport");
        io.AddMouseButtonEvent(0, false);
        frame(draw);
        ImGui::SetScrollY(listWindow, 0);
        frame(draw);
        frame(draw);
        io.AddMousePosEvent(firstButton.x, firstButton.y);
        io.AddMouseButtonEvent(0, true);
        frame(draw);
        io.AddMouseButtonEvent(0, false);
        frame(draw);
        check(std::get<PropertyCustom>(std::get<PropertyArray>(model[0].value).values[0]).json == "1",
              "custom collection editor commits a validated parent transaction");
        model[0].readOnly = true;
        options.editCustom = [](ItemId, const std::vector<std::string> &, const PropertyCustom &v) {
            ImGui::Button("Read only");
            return Edit<PropertyCustom>{EditPhase::commit, {v.type, "99"}};
        };
        frame([&] {
            check(inspector("large", state, model, 1, {}, options).empty(),
                  "read-only collection suppresses custom editor proposals");
        });
        check(std::get<PropertyCustom>(std::get<PropertyArray>(state.drafts[500].value).values[0]).json ==
                  "1",
              "read-only custom drafts remain unchanged");
    }
    for (auto density : {Density::compact, Density::comfortable}) {
        setDensity(density);
        std::vector<PropertyValue> kinds{true,
                                         INT64_MAX,
                                         2.56f,
                                         2.56,
                                         std::string("Text"),
                                         PropertyVector{{1, 2}},
                                         PropertyVector{{1, 2, 3}},
                                         PropertyVector{{1, 2, 3, 4}},
                                         PropertyColor{},
                                         PropertyAsset{"Texture", "/Game/Test"},
                                         makePropertyArray(2.56, 3),
                                         record,
                                         PropertyCustom{"Angle", "45"}};
        for (const auto &kind : kinds) {
            Property field;
            field.id = 501;
            field.label = "Typed array";
            field.value = makePropertyArray(kind, 100);
            std::vector<Property> model{field};
            InspectorState state;
            InspectorOptions options;
            options.arrayHeight = 150;
            frame([&] { inspector("kinds", state, model, 0, {}, options); });
            frame([&] { inspector("kinds", state, model, 0, {}, options); });
            check(!state.lists.empty() && state.lists.begin()->second.renderedRows > 0 &&
                      state.lists.begin()->second.renderedRows < 20,
                  "every built-in, nested, object and custom array type uses the same virtual rows at both "
                  "densities");
        }
    }
    setDensity(Density::compact);
    {
        Property field = p;
        field.id = 502;
        field.category.clear();
        field.label = "Numeric elements";
        field.value = makePropertyArray(2.56f, 10000);
        std::vector<Property> model{field};
        InspectorState state;
        auto draw = [&] {
            for (const auto &event : inspector("numericArray", state, model))
                if (event.edit.committed())
                    model[0].value = event.edit.value;
        };
        frame(draw);
        frame(draw);
        ImGuiWindow *child = nullptr;
        for (auto *window : ImGui::GetCurrentContext()->Windows)
            if (window->LastFrameActive == ImGui::GetFrameCount() && std::strstr(window->Name, "##array"))
                child = window;
        check(child != nullptr, "numeric array has its own virtual viewport");
        if (child) {
            const ImVec2 mouse{child->Pos.x + dp(100), child->Pos.y + ImGui::GetStyle().WindowPadding.y +
                                                           ImGui::GetFrameHeight() * .5f};
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
            bool open = false;
            for (const auto &entry : state.elements)
                open |= entry.second.expressionOpen;
            check(open, "double-click numeric array element opens expression entry");
            io.AddInputCharactersUTF8("Value*100");
            frame(draw);
            io.AddKeyEvent(ImGuiKey_Enter, true);
            frame(draw);
            io.AddKeyEvent(ImGuiKey_Enter, false);
            frame(draw);
            check(std::get<float>(std::get<PropertyArray>(model[0].value).values[0]) == 10,
                  "float array expression commits and clamps the parent property");
        }
    }
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
