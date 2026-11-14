#pragma once
#include "cyui/Core.h"
#include "cyui/Interaction.h"
#include <functional>
#include <limits>
#include <string>
#include <unordered_map>
#include <variant>
#include <array>
#include <optional>
#include <unordered_set>
#include <map>
#include <memory>
#include "cyui/Numeric.h"

namespace cyui {
struct PropertyVector {
    std::vector<double> values;
    bool operator==(const PropertyVector &b) const {
        return values == b.values;
    }
};
struct PropertyColor {
    std::array<float, 4> rgba{0, 0, 0, 1};
    bool operator==(const PropertyColor &b) const {
        return rgba == b.rgba;
    }
};
struct PropertyAsset {
    std::string type, path;
    bool operator==(const PropertyAsset &b) const {
        return type == b.type && path == b.path;
    }
};
struct PropertyArray;
struct PropertyObject;
// A host-defined value with a stable type identifier and a JSON payload.
struct PropertyCustom {
    std::string type, json = "null";
    bool operator==(const PropertyCustom &b) const {
        return type == b.type && json == b.json;
    }
};
// Recursive containers have value semantics. Existing scalar alternative indices are preserved.
using PropertyValue =
    std::variant<bool, std::int64_t, double, std::string, float, PropertyVector, PropertyColor, PropertyAsset,
                 PropertyArray, PropertyObject, PropertyCustom>;
using PropertyAtom = PropertyValue; // source-compatible name; elements may now be recursive
struct PropertyArray {
    std::vector<PropertyValue> values;
    // Immutable schema/default for Add and empty collections. Otherwise the first element is used.
    std::shared_ptr<const PropertyValue> elementTemplate;
    bool operator==(const PropertyArray &b) const;
};
struct PropertyObject {
    std::string type;
    std::map<std::string, PropertyValue> fields; // stable, untranslated field keys
    bool operator==(const PropertyObject &b) const;
};
inline bool PropertyArray::operator==(const PropertyArray &b) const {
    return values == b.values && bool(elementTemplate) == bool(b.elementTemplate) &&
           (!elementTemplate || *elementTemplate == *b.elementTemplate);
}
inline bool PropertyObject::operator==(const PropertyObject &b) const {
    return type == b.type && fields == b.fields;
}
CYUI_API PropertyArray makePropertyArray(PropertyValue elementTemplate, std::size_t count = 0);
struct Property {
    ItemId id = 0;
    std::string label, help;
    PropertyValue value;
    bool readOnly = false, password = false, multiline = false, bounded = false;
    double minimum = 0, maximum = 1;
    std::int64_t integerMinimum = std::numeric_limits<std::int64_t>::lowest(),
                 integerMaximum = std::numeric_limits<std::int64_t>::max();
    std::vector<std::string> choices;
    std::string key, category; // stable clipboard key; defaults to label when empty
    bool mixed = false;        // value represents the first of several selected objects
    std::optional<PropertyValue> defaultValue;
};
struct PropertyEdit {
    ItemId id = 0;
    Edit<PropertyValue> edit;
};
struct PropertyDraft {
    PropertyValue value, before;
    bool active = false;
    std::string error;
    bool cancelled = false;
    std::string expression;
    bool expressionOpen = false, expressionFresh = false;
    bool initialized = false;
    ImGuiID activeId = 0;
};
struct PropertyListState {
    int activeRow = -1;
    std::size_t renderedRows = 0; // last frame; useful for profiling large collections
};
struct InspectorState {
    std::unordered_map<ItemId, PropertyDraft> drafts;
    std::uint64_t revision = 0;
    std::string clipboardError;
    std::unordered_set<std::string> collapsedCategories;
    std::unordered_map<ImGuiID, PropertyListState> lists;
    std::unordered_map<ImGuiID, PropertyDraft> elements;
};
using PropertyValidator = std::function<std::string(ItemId, const PropertyValue &)>;
struct InspectorOptions {
    const TextFilter *filter = nullptr;
    const char *filterContext = nullptr; // optional section/path, borrowed during the call
    bool clipboard = true, categories = true;
    std::function<void(ItemId)> browseAsset; // host selects/loads resources
    float arrayHeight = 180;                 // logical units; dp() is applied by the view
    // Custom editors must draw one frame-height row. Commit only on a completed edit;
    // the inspector normalizes the JSON and validates the entire parent property.
    // path contains array indices / object keys, from the root property to this value.
    std::function<Edit<PropertyCustom>(ItemId, const std::vector<std::string> &path, const PropertyCustom &)>
        editCustom;
    std::function<void(ItemId, const std::vector<std::string> &path)> browseAssetAt;
};
struct PropertyPasteResult {
    std::vector<PropertyEdit> edits; // all-or-nothing validated commits; model remains host-owned
    std::string error;
    explicit operator bool() const {
        return error.empty() && !edits.empty();
    }
};
CYUI_API std::string propertyType(const PropertyValue &);
CYUI_API std::string propertyToJson(const Property &);
CYUI_API std::string propertyCategoryToJson(ArrayView<Property>, const std::string &category);
CYUI_API PropertyPasteResult propertyFromJson(const Property &, const std::string &,
                                              const PropertyValidator & = {});
CYUI_API PropertyPasteResult propertyCategoryFromJson(ArrayView<Property>, const std::string &category,
                                                      const std::string &, const PropertyValidator & = {});
// Validates types/shape, rejects non-finite values, clamps bounds, then invokes the host validator.
CYUI_API bool normalizePropertyValue(const Property &, PropertyValue &, std::string &error);
CYUI_API bool evaluatePropertyExpression(const Property &, const std::string &, PropertyValue &,
                                         std::string &error);
// Combines matching keys/types across objects. Applying each returned commit to all matching
// objects is one host Undo transaction; heterogeneous/missing fields are omitted.
CYUI_API std::vector<Property> commonProperties(const std::vector<std::vector<Property>> &objects);
CYUI_API bool propertyMatches(const TextFilter &, const Property &, const char *context = nullptr);
CYUI_API std::vector<PropertyEdit> inspector(const char *id, InspectorState &, ArrayView<Property>,
                                             std::uint64_t revision = 0, const PropertyValidator & = {});
CYUI_API std::vector<PropertyEdit> inspector(const char *id, InspectorState &, ArrayView<Property>,
                                             std::uint64_t revision, const PropertyValidator &,
                                             const InspectorOptions &);
} // namespace cyui
