#include "cyui/modules/Inspector.h"
#include "cyui/detail/Json.h"
#include <unordered_set>
#include <charconv>
#include <type_traits>
#include <climits>

namespace cyui {
namespace {
using detail::Json;
std::string key(const Property &p) {
    return p.key.empty() ? p.label : p.key;
}
template <class V> Json encode(const V &value, int depth = 0) {
    if (depth > 12)
        throw std::invalid_argument("Property nesting exceeds 12 levels");
    return std::visit(
        [depth](const auto &v) -> Json {
            using T = std::decay_t<decltype(v)>;
            Json j = Json::obj();
            if constexpr (std::is_same_v<T, bool>) {
                j.fields["type"] = Json::str("bool");
                j.fields["value"] = Json::flag(v);
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                j.fields["type"] = Json::str("int64");
                j.fields["value"] = Json::num(std::to_string(v));
            } else if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>) {
                j.fields["type"] = Json::str(std::is_same_v<T, float> ? "float" : "double");
                j.fields["value"] = Json::num(v);
            } else if constexpr (std::is_same_v<T, std::string>) {
                j.fields["type"] = Json::str("string");
                j.fields["value"] = Json::str(v);
            } else if constexpr (std::is_same_v<T, PropertyVector>) {
                j.fields["type"] = Json::str("vector" + std::to_string(v.values.size()));
                auto a = Json::list();
                for (auto n : v.values)
                    a.items.push_back(Json::num(n));
                j.fields["value"] = std::move(a);
            } else if constexpr (std::is_same_v<T, PropertyColor>) {
                j.fields["type"] = Json::str("color");
                auto a = Json::list();
                for (auto n : v.rgba)
                    a.items.push_back(Json::num(n));
                j.fields["value"] = std::move(a);
            } else if constexpr (std::is_same_v<T, PropertyAsset>) {
                j.fields["type"] = Json::str("asset");
                j.fields["assetType"] = Json::str(v.type);
                j.fields["value"] = Json::str(v.path);
            } else if constexpr (std::is_same_v<T, PropertyCustom>) {
                j.fields["type"] = Json::str("custom");
                j.fields["customType"] = Json::str(v.type);
                j.fields["value"] = detail::jsonParse(v.json);
            } else if constexpr (std::is_same_v<T, PropertyObject>) {
                j.fields["type"] = Json::str("object");
                j.fields["objectType"] = Json::str(v.type);
                auto fields = Json::obj();
                for (const auto &field : v.fields)
                    fields.fields[field.first] = encode(field.second, depth + 1);
                j.fields["value"] = std::move(fields);
            } else {
                j.fields["type"] = Json::str("array");
                if (v.elementTemplate)
                    j.fields["element"] = encode(*v.elementTemplate, depth + 1);
                auto a = Json::list();
                for (const auto &atom : v.values)
                    a.items.push_back(encode(atom, depth + 1));
                j.fields["value"] = std::move(a);
            }
            return j;
        },
        value);
}
PropertyValue decode(const Json &j, int depth = 0) {
    if (depth > 12)
        throw std::invalid_argument("Property nesting exceeds 12 levels");
    auto type = j.get("type").str();
    const auto &v = j.get("value");
    if (type == "bool")
        return v.flag();
    if (type == "int64")
        return v.integer<std::int64_t>();
    if (type == "double")
        return double(v.num());
    if (type == "float")
        return float(v.num());
    if (type == "string")
        return v.str();
    if (type == "asset")
        return PropertyAsset{j.get("assetType").str(), v.str()};
    if (type == "custom")
        return PropertyCustom{j.get("customType").str(), detail::jsonDump(v)};
    if (type == "object") {
        if (v.kind != Json::object)
            throw std::invalid_argument("Invalid object");
        PropertyObject object{j.get("objectType").str(), {}};
        for (const auto &field : v.fields)
            object.fields[field.first] = decode(field.second, depth + 1);
        return object;
    }
    if (type == "color") {
        if (v.kind != Json::array || v.items.size() != 4)
            throw std::invalid_argument("Invalid color");
        PropertyColor c;
        for (int i = 0; i < 4; ++i)
            c.rgba[i] = float(v.items[i].num());
        return c;
    }
    if (type == "vector2" || type == "vector3" || type == "vector4") {
        if (v.kind != Json::array || v.items.size() != std::size_t(type.back() - '0'))
            throw std::invalid_argument("Invalid vector");
        PropertyVector p;
        for (const auto &n : v.items)
            p.values.push_back(double(n.num()));
        return p;
    }
    if (type == "array") {
        if (v.kind != Json::array)
            throw std::invalid_argument("Invalid collection");
        PropertyArray a;
        if (j.fields.count("element"))
            a.elementTemplate = std::make_shared<PropertyValue>(decode(j.get("element"), depth + 1));
        for (const auto &item : v.items)
            a.values.push_back(decode(item, depth + 1));
        return a;
    }
    throw std::invalid_argument("Unknown property type");
}
Json envelope(const char *kind) {
    auto j = Json::obj();
    j.fields["format"] = Json::str("cyui.inspector");
    j.fields["version"] = Json::num("1");
    j.fields["kind"] = Json::str(kind);
    return j;
}
Json read(const std::string &text, const char *kind) {
    auto j = detail::jsonParse(text);
    if (j.get("format").str() != "cyui.inspector" || j.get("version").integer<int>() != 1 ||
        j.get("kind").str() != kind)
        throw std::invalid_argument("Unsupported clipboard format");
    return j;
}
PropertyEdit checked(const Property &p, PropertyValue v, const PropertyValidator &validate) {
    std::string error;
    if (p.readOnly)
        throw std::invalid_argument("Property is read-only");
    if (!normalizePropertyValue(p, v, error))
        throw std::invalid_argument(error);
    if (validate) {
        error = validate(p.id, v);
        if (!error.empty())
            throw std::invalid_argument(error);
    }
    return {p.id, {EditPhase::commit, std::move(v)}};
}
} // namespace
PropertyArray makePropertyArray(PropertyValue elementTemplate, std::size_t count) {
    if (count > std::size_t(INT_MAX))
        throw std::length_error("Too many collection rows");
    PropertyArray result;
    result.elementTemplate = std::make_shared<const PropertyValue>(std::move(elementTemplate));
    result.values.assign(count, *result.elementTemplate);
    return result;
}
std::string propertyType(const PropertyValue &v) {
    return std::visit(
        [](const auto &n) -> std::string {
            using T = std::decay_t<decltype(n)>;
            if constexpr (std::is_same_v<T, bool>)
                return "bool";
            else if constexpr (std::is_same_v<T, std::int64_t>)
                return "int64";
            else if constexpr (std::is_same_v<T, double>)
                return "double";
            else if constexpr (std::is_same_v<T, float>)
                return "float";
            else if constexpr (std::is_same_v<T, std::string>)
                return "string";
            else if constexpr (std::is_same_v<T, PropertyVector>)
                return "vector" + std::to_string(n.values.size());
            else if constexpr (std::is_same_v<T, PropertyColor>)
                return "color";
            else if constexpr (std::is_same_v<T, PropertyAsset>)
                return "asset:" + n.type;
            else if constexpr (std::is_same_v<T, PropertyObject>)
                return "object:" + n.type;
            else if constexpr (std::is_same_v<T, PropertyCustom>)
                return "custom:" + n.type;
            else
                return "array";
        },
        v);
}
static bool normalizeAt(const Property &p, PropertyValue &v, std::string &error, int depth) {
    error.clear();
    try {
        if (depth > 12)
            throw std::invalid_argument("Property nesting exceeds 12 levels");
        if (propertyType(p.value) != propertyType(v))
            throw std::invalid_argument("Incompatible property type");
        auto number = [&](auto &n) {
            using T = std::decay_t<decltype(n)>;
            if (!std::isfinite(n))
                throw std::invalid_argument("Non-finite value");
            if (p.bounded) {
                if (!std::isfinite(p.minimum) || !std::isfinite(p.maximum) || p.minimum > p.maximum)
                    throw std::invalid_argument("Invalid numeric bounds");
                n = T(std::clamp(double(n), p.minimum, p.maximum));
                if (!std::isfinite(n))
                    throw std::invalid_argument("Numeric bound exceeds target type");
            }
        };
        std::visit(
            [&](auto &n) {
                using T = std::decay_t<decltype(n)>;
                if constexpr (std::is_same_v<T, std::int64_t>) {
                    if (p.bounded) {
                        if (p.integerMinimum > p.integerMaximum)
                            throw std::invalid_argument("Invalid integer bounds");
                        n = std::clamp(n, p.integerMinimum, p.integerMaximum);
                    }
                } else if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>)
                    number(n);
                else if constexpr (std::is_same_v<T, PropertyVector>) {
                    if (n.values.size() < 2 || n.values.size() > 4)
                        throw std::invalid_argument("Expected vector2, vector3 or vector4");
                    for (auto &c : n.values)
                        number(c);
                } else if constexpr (std::is_same_v<T, PropertyColor>) {
                    for (auto &c : n.rgba) {
                        if (!std::isfinite(c))
                            throw std::invalid_argument("Non-finite color");
                        c = std::clamp(c, 0.f, 1.f);
                    }
                } else if constexpr (std::is_same_v<T, std::string>) {
                    if (!p.choices.empty() &&
                        std::find(p.choices.begin(), p.choices.end(), n) == p.choices.end())
                        throw std::invalid_argument("Value is not an available choice");
                } else if constexpr (std::is_same_v<T, PropertyArray>) {
                    if (n.values.size() > std::size_t(INT_MAX))
                        throw std::invalid_argument("Too many collection rows");
                    const auto &source = std::get<PropertyArray>(p.value);
                    const PropertyValue *prototype =
                        source.elementTemplate ? source.elementTemplate.get()
                                               : (source.values.empty() ? nullptr : &source.values.front());
                    if (!prototype)
                        prototype = n.elementTemplate ? n.elementTemplate.get()
                                                      : (n.values.empty() ? nullptr : &n.values.front());
                    std::optional<PropertyValue> stablePrototype;
                    if (prototype) {
                        stablePrototype = *prototype;
                        prototype = &*stablePrototype;
                    }
                    auto normalizeElement = [&](PropertyValue &element) {
                        Property field;
                        field.value = *prototype;
                        field.bounded = p.bounded;
                        field.minimum = p.minimum;
                        field.maximum = p.maximum;
                        field.integerMinimum = p.integerMinimum;
                        field.integerMaximum = p.integerMaximum;
                        field.choices = p.choices;
                        std::string e;
                        if (!normalizeAt(field, element, e, depth + 1))
                            throw std::invalid_argument(e);
                    };
                    if (prototype && n.elementTemplate) {
                        auto element = *n.elementTemplate;
                        normalizeElement(element);
                        n.elementTemplate = std::make_shared<const PropertyValue>(std::move(element));
                    }
                    for (auto &atom : n.values) {
                        normalizeElement(atom);
                    }
                    if (source.elementTemplate)
                        n.elementTemplate = source.elementTemplate;
                } else if constexpr (std::is_same_v<T, PropertyObject>) {
                    const auto &source = std::get<PropertyObject>(p.value);
                    if (n.type.empty() || source.fields.size() != n.fields.size())
                        throw std::invalid_argument("Object schema mismatch");
                    for (auto &field : n.fields) {
                        const auto target = source.fields.find(field.first);
                        if (target == source.fields.end())
                            throw std::invalid_argument("Object schema mismatch");
                        Property child;
                        child.value = target->second;
                        child.bounded = p.bounded;
                        child.minimum = p.minimum;
                        child.maximum = p.maximum;
                        child.integerMinimum = p.integerMinimum;
                        child.integerMaximum = p.integerMaximum;
                        child.choices = p.choices;
                        std::string e;
                        if (!normalizeAt(child, field.second, e, depth + 1))
                            throw std::invalid_argument(e);
                    }
                } else if constexpr (std::is_same_v<T, PropertyCustom>) {
                    if (n.type.empty())
                        throw std::invalid_argument("Custom type identifier is required");
                    n.json = detail::jsonDump(detail::jsonParse(n.json));
                }
            },
            v);
        return true;
    } catch (const std::invalid_argument &e) {
        error = e.what();
        return false;
    }
}
bool normalizePropertyValue(const Property &p, PropertyValue &v, std::string &error) {
    return normalizeAt(p, v, error, 0);
}
bool evaluatePropertyExpression(const Property &p, const std::string &expression, PropertyValue &out,
                                std::string &error) {
    out = p.value;
    error.clear();
    if (auto integer = std::get_if<std::int64_t>(&out)) {
        auto r = std::from_chars(expression.data(), expression.data() + expression.size(), *integer);
        if (r.ec != std::errc() || r.ptr != expression.data() + expression.size()) {
            auto original = std::get<std::int64_t>(p.value);
            if (std::numeric_limits<long double>::digits <= 53 &&
                (original > 9007199254740991LL || original < -9007199254740991LL)) {
                error = "Use an exact integer literal for values beyond 2^53";
                return false;
            }
            auto e = evaluateNumericExpression(expression, static_cast<long double>(original));
            if (!e.valid) {
                error = e.error;
                return false;
            }
            auto n = std::round(e.value);
            if (p.bounded)
                n = std::clamp(n, static_cast<long double>(p.integerMinimum),
                               static_cast<long double>(p.integerMaximum));
            if (n < -9223372036854775808.L || n >= 9223372036854775808.L) {
                error = "Integer overflow";
                return false;
            }
            *integer = static_cast<std::int64_t>(n);
        }
    } else if (std::holds_alternative<double>(out) || std::holds_alternative<float>(out)) {
        const auto original =
            std::holds_alternative<double>(out) ? std::get<double>(out) : std::get<float>(out);
        auto e = evaluateNumericExpression(expression, original);
        if (!e.valid) {
            error = e.error;
            return false;
        }
        if (p.bounded) {
            if (p.minimum > p.maximum) {
                error = "Invalid numeric bounds";
                return false;
            }
            e.value =
                std::clamp(e.value, static_cast<long double>(p.minimum), static_cast<long double>(p.maximum));
        }
        if (std::holds_alternative<double>(out))
            out = double(e.value);
        else
            out = float(e.value);
    } else {
        error = "Expected a numeric property";
        return false;
    }
    return normalizePropertyValue(p, out, error);
}
std::string propertyToJson(const Property &p) {
    auto j = envelope("property");
    j.fields["property"] = encode(p.value);
    auto text = detail::jsonDump(j);
    read(text, "property"); // exported clipboard data must fit the same budgets as import
    return text;
}
std::string propertyCategoryToJson(ArrayView<Property> fields, const std::string &category) {
    auto j = envelope("category");
    auto values = Json::obj();
    for (std::size_t i = 0; i < fields.count; ++i)
        if (fields[i].category == category) {
            if (!values.fields.emplace(key(fields[i]), encode(fields[i].value)).second)
                throw std::invalid_argument("Duplicate clipboard key");
        }
    j.fields["properties"] = std::move(values);
    auto text = detail::jsonDump(j);
    read(text, "category");
    return text;
}
PropertyPasteResult propertyFromJson(const Property &p, const std::string &text,
                                     const PropertyValidator &validate) {
    try {
        return {{checked(p, decode(read(text, "property").get("property")), validate)}, {}};
    } catch (const std::exception &e) {
        return {{}, e.what()};
    }
}
PropertyPasteResult propertyCategoryFromJson(ArrayView<Property> fields, const std::string &category,
                                             const std::string &text, const PropertyValidator &validate) {
    try {
        auto j = read(text, "category");
        const auto &values = j.get("properties");
        if (values.kind != Json::object)
            throw std::invalid_argument("Expected property map");
        PropertyPasteResult r;
        std::unordered_set<std::string> keys;
        for (std::size_t i = 0; i < fields.count; ++i)
            if (fields[i].category == category) {
                const auto &p = fields[i];
                if (!keys.insert(key(p)).second)
                    throw std::invalid_argument("Duplicate clipboard key");
                auto found = values.fields.find(key(p));
                if (found == values.fields.end())
                    throw std::invalid_argument("Category property is missing");
                r.edits.push_back(checked(p, decode(found->second), validate));
            }
        if (r.edits.empty() || r.edits.size() != values.fields.size())
            throw std::invalid_argument("Category schema does not match");
        return r;
    } catch (const std::exception &e) {
        return {{}, e.what()};
    }
}
static bool sameSchema(const PropertyValue &a, const PropertyValue &b, int depth = 0) {
    if (depth > 12 || propertyType(a) != propertyType(b))
        return false;
    if (auto array = std::get_if<PropertyArray>(&a)) {
        const auto &other = std::get<PropertyArray>(b);
        const auto *left = array->elementTemplate
                               ? array->elementTemplate.get()
                               : (array->values.empty() ? nullptr : &array->values.front());
        const auto *right = other.elementTemplate ? other.elementTemplate.get()
                                                  : (other.values.empty() ? nullptr : &other.values.front());
        return !left || !right || sameSchema(*left, *right, depth + 1);
    }
    if (auto object = std::get_if<PropertyObject>(&a)) {
        const auto &other = std::get<PropertyObject>(b);
        if (object->fields.size() != other.fields.size())
            return false;
        for (const auto &field : object->fields) {
            const auto target = other.fields.find(field.first);
            if (target == other.fields.end() || !sameSchema(field.second, target->second, depth + 1))
                return false;
        }
    }
    return true;
}
std::vector<Property> commonProperties(const std::vector<std::vector<Property>> &objects) {
    if (objects.empty())
        return {};
    std::vector<Property> result;
    for (auto p : objects.front()) {
        bool common = true;
        for (std::size_t i = 1; i < objects.size(); ++i) {
            auto found = std::find_if(objects[i].begin(), objects[i].end(), [&](const auto &v) {
                return v.category == p.category && key(v) == key(p);
            });
            if (found == objects[i].end() || !sameSchema(p.value, found->value)) {
                common = false;
                break;
            }
            p.mixed |= !(found->value == p.value);
            p.readOnly |= found->readOnly;
            if (found->bounded) {
                if (!p.bounded) {
                    p.minimum = found->minimum;
                    p.maximum = found->maximum;
                    p.integerMinimum = found->integerMinimum;
                    p.integerMaximum = found->integerMaximum;
                } else {
                    p.minimum = std::max(p.minimum, found->minimum);
                    p.maximum = std::min(p.maximum, found->maximum);
                    p.integerMinimum = std::max(p.integerMinimum, found->integerMinimum);
                    p.integerMaximum = std::min(p.integerMaximum, found->integerMaximum);
                }
                p.bounded = true;
            }
            if (!found->choices.empty()) {
                if (p.choices.empty())
                    p.choices = found->choices;
                else
                    p.choices.erase(std::remove_if(p.choices.begin(), p.choices.end(),
                                                   [&](const auto &choice) {
                                                       return std::find(found->choices.begin(),
                                                                        found->choices.end(),
                                                                        choice) == found->choices.end();
                                                   }),
                                    p.choices.end());
                if (p.choices.empty()) {
                    common = false;
                    break;
                }
            }
        }
        if (p.bounded && (std::holds_alternative<std::int64_t>(p.value) ? p.integerMinimum > p.integerMaximum
                                                                        : p.minimum > p.maximum))
            common = false;
        if (common)
            result.push_back(std::move(p));
    }
    return result;
}
} // namespace cyui
