#include "cyui/modules/Inspector.h"
#include "ModuleInternal.h"
#include <unordered_set>
#include <cfloat>
#include <sstream>
#include <iomanip>
#include <locale>
#include <climits>

namespace cyui {
namespace {
struct DrawEdit {
    bool changed = false, immediate = false, activated = false, active = false;
    bool deactivated = false, edited = false;
    bool cancelled = false;
    void observe() {
        activated |= ImGui::IsItemActivated();
        active |= ImGui::IsItemActive();
        deactivated |= ImGui::IsItemDeactivated();
        edited |= ImGui::IsItemDeactivatedAfterEdit();
    }
    void merge(const DrawEdit &b) {
        changed |= b.changed;
        immediate |= b.immediate;
        activated |= b.activated;
        active |= b.active;
        deactivated |= b.deactivated;
        edited |= b.edited;
        cancelled |= b.cancelled;
    }
};
Property childField(const Property &root, const PropertyValue &value) {
    Property field;
    field.id = root.id;
    field.value = value;
    field.readOnly = root.readOnly;
    field.bounded = root.bounded;
    field.minimum = root.minimum;
    field.maximum = root.maximum;
    field.integerMinimum = root.integerMinimum;
    field.integerMaximum = root.integerMaximum;
    field.choices = root.choices;
    field.password = root.password;
    return field;
}
std::string numberText(const PropertyValue &value) {
    return std::visit(
        [](const auto &n) -> std::string {
            using T = std::decay_t<decltype(n)>;
            if constexpr (std::is_arithmetic_v<T>) {
                std::ostringstream text;
                text.imbue(std::locale::classic());
                text << std::setprecision(std::numeric_limits<T>::max_digits10) << n;
                return text.str();
            } else
                return {};
        },
        value);
}
DrawEdit drawValue(PropertyValue &, const Property &, const InspectorOptions &, InspectorState &,
                   std::vector<std::string> &, bool = false);
DrawEdit drawArray(PropertyArray &array, const Property &root, const InspectorOptions &options,
                   InspectorState &state, std::vector<std::string> &path) {
    DrawEdit out;
    const auto listId = ImGui::GetID("##array");
    auto &list = state.lists[listId];
    list.renderedRows = 0;
    int action = 0, selected = -1;
    if (ImGui::BeginChild("##array", {0, dp(std::max(60.f, options.arrayHeight))}, ImGuiChildFlags_Borders)) {
        const auto count = int(std::min(array.values.size(), std::size_t(INT_MAX)));
        const auto pitch = ImGui::GetFrameHeightWithSpacing();
        ImGuiListClipper clip;
        clip.Begin(count, pitch);
        if (list.activeRow >= 0 && list.activeRow < count)
            clip.IncludeItemByIndex(list.activeRow);
        while (clip.Step())
            for (int row = clip.DisplayStart; row < clip.DisplayEnd; ++row) {
                ++list.renderedRows;
                ScopedID scope(row);
                const float y = ImGui::GetCursorPosY();
                ImGui::AlignTextToFramePadding();
                textDim("%d", row);
                ImGui::SameLine(dp(52));
                ImGui::SetNextItemWidth(-dp(30));
                path.push_back(std::to_string(row));
                auto edit = drawValue(array.values[row], root, options, state, path);
                path.pop_back();
                out.merge(edit);
                if (edit.active || edit.activated)
                    list.activeRow = row;
                else if (list.activeRow == row)
                    list.activeRow = -1;
                if (options.clipboard && ImGui::BeginPopupContextItem("##elementMenu")) {
                    const auto field = childField(root, array.values[row]);
                    if (ImGui::MenuItem(trLabel("Copy as JSON").c_str(), nullptr, false, !root.password)) {
                        try {
                            ImGui::SetClipboardText(propertyToJson(field).c_str());
                        } catch (const std::exception &e) {
                            state.clipboardError = e.what();
                        }
                    }
                    if (ImGui::MenuItem(trLabel("Paste value").c_str(), nullptr, false, !root.readOnly)) {
                        const auto text = ImGui::GetClipboardText();
                        auto paste = propertyFromJson(field, text ? text : "");
                        state.clipboardError = paste.error;
                        if (paste) {
                            array.values[row] = paste.edits.front().edit.value;
                            out.changed = out.immediate = true;
                        }
                    }
                    const char *labels[]{"Duplicate", "Move up", "Move down", "Remove"};
                    for (int n = 0; n < 4; ++n)
                        if (ImGui::MenuItem(trLabel(labels[n]).c_str(), nullptr, false,
                                            !root.readOnly && (n != 1 || row > 0) &&
                                                (n != 2 || row + 1 < count))) {
                            action = n + 1;
                            selected = row;
                        }
                    ImGui::EndPopup();
                }
                ImGui::SameLine();
                if (ghostButton("-")) {
                    action = 4;
                    selected = row;
                }
                ImGui::SetCursorPosY(y + pitch - ImGui::GetStyle().ItemSpacing.y);
                ImGui::Dummy({0, 0});
            }
    }
    ImGui::EndChild();
    if (action && selected >= 0) {
        auto at = array.values.begin() + selected;
        if (action == 1 && array.values.size() < std::size_t(INT_MAX)) {
            auto value = *at;
            array.values.insert(at + 1, std::move(value));
        } else if (action == 2)
            std::swap(*at, *(at - 1));
        else if (action == 3)
            std::swap(*at, *(at + 1));
        else if (action == 4)
            array.values.erase(at);
        state.elements.clear();
        list.activeRow = -1;
        ImGui::ClearActiveID();
        out.changed = out.immediate = true;
    }
    if (ghostButton(trLabel("Add element").c_str()) && array.values.size() < std::size_t(INT_MAX)) {
        // Copy before insertion: the source may point into the vector being reallocated.
        PropertyValue added = array.elementTemplate ? *array.elementTemplate
                                                    : (array.values.empty() ? PropertyValue{std::string{}}
                                                                            : array.values.back());
        array.values.push_back(std::move(added));
        out.changed = out.immediate = true;
    }
    ImGui::SameLine();
    textDim("%zu", array.values.size());
    return out;
}
DrawEdit drawValue(PropertyValue &value, const Property &root, const InspectorOptions &options,
                   InspectorState &state, std::vector<std::string> &path, bool fullArray) {
    DrawEdit out;
    const bool numeric = std::holds_alternative<std::int64_t>(value) ||
                         std::holds_alternative<float>(value) || std::holds_alternative<double>(value);
    ImGui::BeginGroup();
    if (auto b = std::get_if<bool>(&value)) {
        out.changed = ImGui::Checkbox("##value", b);
        out.immediate = true;
        out.observe();
    } else if (auto integer = std::get_if<std::int64_t>(&value)) {
        out.changed = ImGui::DragScalar("##value", ImGuiDataType_S64, integer, 1,
                                        root.bounded ? &root.integerMinimum : nullptr,
                                        root.bounded ? &root.integerMaximum : nullptr, "%lld",
                                        ImGuiSliderFlags_NoInput | ImGuiSliderFlags_AlwaysClamp);
        out.observe();
    } else if (auto number = std::get_if<double>(&value)) {
        out.changed =
            ImGui::DragScalar("##value", ImGuiDataType_Double, number, .01f,
                              root.bounded ? &root.minimum : nullptr, root.bounded ? &root.maximum : nullptr,
                              "%.6g", ImGuiSliderFlags_NoInput | ImGuiSliderFlags_AlwaysClamp);
        out.observe();
    } else if (auto single = std::get_if<float>(&value)) {
        const float low = float(root.minimum), high = float(root.maximum);
        out.changed = ImGui::DragScalar("##value", ImGuiDataType_Float, single, .01f,
                                        root.bounded ? &low : nullptr, root.bounded ? &high : nullptr, "%.6g",
                                        ImGuiSliderFlags_NoInput | ImGuiSliderFlags_AlwaysClamp);
        out.observe();
    } else if (auto s = std::get_if<std::string>(&value)) {
        if (!root.choices.empty()) {
            if (ImGui::BeginCombo("##value", tr(s->c_str()).c_str())) {
                for (const auto &choice : root.choices)
                    if (ImGui::Selectable(trLabel(choice.c_str()).c_str(), choice == *s)) {
                        *s = choice;
                        out.changed = out.immediate = true;
                    }
                ImGui::EndCombo();
            }
        } else
            out.changed = detail::inputText("##value", *s, root.password ? ImGuiInputTextFlags_Password : 0);
        out.observe();
    } else if (auto v = std::get_if<PropertyVector>(&value)) {
        const auto width =
            (ImGui::CalcItemWidth() - ImGui::GetStyle().ItemInnerSpacing.x * float(v->values.size() - 1)) /
            float(std::max(std::size_t(1), v->values.size()));
        for (std::size_t component = 0; component < v->values.size(); ++component) {
            ScopedID scope{int(component)};
            if (component)
                ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::SetNextItemWidth(width);
            PropertyValue componentValue = v->values[component];
            path.push_back(std::to_string(component));
            auto edit = drawValue(componentValue, root, options, state, path);
            path.pop_back();
            if (edit.changed)
                v->values[component] = std::get<double>(componentValue);
            out.merge(edit);
        }
    } else if (auto color = std::get_if<PropertyColor>(&value)) {
        out.changed = ImGui::ColorEdit4("##value", color->rgba.data());
        out.observe();
    } else if (auto asset = std::get_if<PropertyAsset>(&value)) {
        const float width = ImGui::CalcItemWidth();
        ImGui::SetNextItemWidth(std::max(dp(30), width - dp(34)));
        out.changed = detail::inputText("##value", asset->path);
        out.observe();
        ImGui::SameLine();
        if (button("...")) {
            if (options.browseAssetAt)
                options.browseAssetAt(root.id, path);
            else if (options.browseAsset)
                options.browseAsset(root.id);
        }
    } else if (auto custom = std::get_if<PropertyCustom>(&value)) {
        if (options.editCustom) {
            const auto edit = options.editCustom(root.id, path, *custom);
            if (edit.phase == EditPhase::update || edit.phase == EditPhase::commit) {
                *custom = edit.value;
                out.changed = true;
            }
            out.immediate = edit.committed();
            out.activated = edit.phase == EditPhase::begin;
            out.cancelled = edit.phase == EditPhase::cancel;
            out.active = edit.phase == EditPhase::begin || edit.phase == EditPhase::update;
            out.observe();
        } else {
            const auto editorId = ImGui::GetID("##jsonState");
            if (button(trLabel("Edit JSON...").c_str())) {
                auto &draft = state.elements[editorId];
                draft.expression = custom->json;
                draft.error.clear();
                draft.expressionOpen = true;
                ImGui::OpenPopup("##json");
            }
            if (ImGui::BeginPopup("##json")) {
                auto &draft = state.elements[editorId];
                if (!draft.expressionOpen || root.readOnly)
                    ImGui::CloseCurrentPopup();
                if (ImGui::IsWindowAppearing()) {
                    draft.expression = custom->json;
                    draft.error.clear();
                }
                detail::inputText("##jsonText", draft.expression, 0, dp(420, 140));
                if (button(trLabel("Apply").c_str()) && draft.expressionOpen && !root.readOnly) {
                    auto candidate = PropertyValue{PropertyCustom{custom->type, draft.expression}};
                    if (normalizePropertyValue(childField(root, value), candidate, draft.error)) {
                        value = std::move(candidate);
                        out.changed = out.immediate = true;
                        draft.expressionOpen = false;
                        ImGui::CloseCurrentPopup();
                    }
                }
                if (!draft.error.empty())
                    textColoured(palette().error, "%s", draft.error.c_str());
                if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                    ImGui::CloseCurrentPopup();
                out.active |= !out.immediate;
                ImGui::EndPopup();
            }
        }
    } else if (auto array = std::get_if<PropertyArray>(&value); array && fullArray)
        out.merge(drawArray(*array, root, options, state, path));
    else {
        const std::string label =
            std::holds_alternative<PropertyArray>(value)
                ? tr("Array") + " (" + std::to_string(std::get<PropertyArray>(value).values.size()) + ")"
                : std::get<PropertyObject>(value).type;
        if (button((label + "###details").c_str()))
            ImGui::OpenPopup("##details");
        ImGui::SetNextWindowSize(dp(520, 0), ImGuiCond_Appearing);
        if (ImGui::BeginPopup("##details")) {
            if (auto nested = std::get_if<PropertyArray>(&value))
                out.merge(drawArray(*nested, root, options, state, path));
            else {
                for (auto &field : std::get<PropertyObject>(value).fields) {
                    ScopedID scope(field.first.c_str());
                    textDim("%s", tr(field.first.c_str()).c_str());
                    ImGui::SetNextItemWidth(-1);
                    path.push_back(field.first);
                    out.merge(drawValue(field.second, root, options, state, path));
                    path.pop_back();
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                ImGui::CloseCurrentPopup();
            out.active |= !out.immediate;
            ImGui::EndPopup();
        }
    }
    if (numeric) {
        const auto editorId = ImGui::GetID("##numericState");
        if (!root.readOnly && ((ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) ||
                               (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter)))) {
            auto &draft = state.elements[editorId];
            draft.before = value;
            draft.expression = numberText(value);
            draft.error.clear();
            draft.expressionOpen = true;
            ImGui::OpenPopup("##number");
        }
        if (ImGui::BeginPopup("##number")) {
            auto &draft = state.elements[editorId];
            if (!draft.expressionOpen)
                ImGui::CloseCurrentPopup();
            if (ImGui::IsWindowAppearing())
                ImGui::SetKeyboardFocusHere();
            textDim("%s", tr("Use Value for the original value").c_str());
            if (detail::inputText("##expressionText", draft.expression,
                                  ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                PropertyValue candidate;
                if (evaluatePropertyExpression(childField(root, draft.before), draft.expression, candidate,
                                               draft.error)) {
                    value = std::move(candidate);
                    out.changed = out.immediate = true;
                    draft.expressionOpen = false;
                    ImGui::CloseCurrentPopup();
                }
            }
            if (!draft.error.empty())
                textColoured(palette().error, "%s", draft.error.c_str());
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                draft.expressionOpen = false;
                ImGui::CloseCurrentPopup();
            }
            out.active |= draft.expressionOpen;
            ImGui::EndPopup();
        }
    }
    ImGui::EndGroup();
    return out;
}
} // namespace
bool propertyMatches(const TextFilter &filter, const Property &property, const char *context) {
    if (!filter.active())
        return true;
    const std::string path = context ? context : "";
    return filter.matches(property.label, path + " " + tr(path.c_str()) + " " + property.help + " " +
                                              tr(property.help.c_str()));
}
std::vector<PropertyEdit> inspector(const char *id, InspectorState &state, ArrayView<Property> properties,
                                    std::uint64_t revision, const PropertyValidator &validate) {
    return inspector(id, state, properties, revision, validate, {});
}
std::vector<PropertyEdit> inspector(const char *id, InspectorState &s, ArrayView<Property> properties,
                                    std::uint64_t revision, const PropertyValidator &validate,
                                    const InspectorOptions &options) {
    std::vector<PropertyEdit> result;
    ScopedID scope(id);
    for (auto &list : s.lists)
        list.second.renderedRows = 0;
    std::unordered_set<ItemId> present;
    if (s.revision != revision) {
        for (auto &entry : s.drafts)
            if (entry.second.active) {
                if (entry.second.activeId && ImGui::GetActiveID() == entry.second.activeId)
                    ImGui::ClearActiveID();
                result.push_back({entry.first, {EditPhase::cancel, entry.second.before}});
            }
        s.drafts.clear();
        s.elements.clear();
        s.lists.clear();
        s.revision = revision;
    }
    if (!beginProperties("##properties"))
        return result;
    std::size_t visible = 0;
    std::unordered_set<std::string> categories;
    for (std::size_t i = 0; i < properties.count; ++i) {
        const auto &p = properties[i];
        present.insert(p.id);
        if (options.filter && !propertyMatches(*options.filter, p, options.filterContext)) {
            const auto it = s.drafts.find(p.id);
            if (it != s.drafts.end() && it->second.active) {
                ItemScope key(p.id);
                if (ImGui::GetActiveID() == ImGui::GetID("##value") ||
                    (it->second.activeId && ImGui::GetActiveID() == it->second.activeId))
                    ImGui::ClearActiveID();
                auto &draft = it->second;
                result.push_back({p.id, {EditPhase::cancel, draft.before}});
                draft.value = draft.before;
                draft.active = false;
                draft.cancelled = true;
                draft.error.clear();
            }
            continue;
        }
        ++visible;
        if (options.categories && !p.category.empty() && categories.insert(p.category).second) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            const auto categoryLabel =
                (s.collapsedCategories.count(p.category) ? "> " : "v ") + trLabel(p.category.c_str());
            if (ImGui::Selectable(categoryLabel.c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
                if (!s.collapsedCategories.erase(p.category))
                    s.collapsedCategories.insert(p.category);
            if (options.clipboard && ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem(trLabel("Copy category as JSON").c_str()))
                    try {
                        ImGui::SetClipboardText(propertyCategoryToJson(properties, p.category).c_str());
                    } catch (const std::exception &e) {
                        s.clipboardError = e.what();
                    }
                if (ImGui::MenuItem(trLabel("Paste category").c_str())) {
                    auto text = ImGui::GetClipboardText();
                    auto paste = propertyCategoryFromJson(properties, p.category, text ? text : "", validate);
                    s.clipboardError = paste.error;
                    if (paste) {
                        result.insert(result.end(), paste.edits.begin(), paste.edits.end());
                        s.drafts.clear();
                        s.elements.clear();
                        for (auto &list : s.lists)
                            list.second.activeRow = -1;
                    }
                }
                ImGui::EndPopup();
            }
        }
        if (options.categories && !p.category.empty() && s.collapsedCategories.count(p.category) &&
            !(options.filter && options.filter->active())) {
            auto found = s.drafts.find(p.id);
            if (found != s.drafts.end() && found->second.active) {
                auto &draft = found->second;
                ItemScope key(p.id);
                if (ImGui::GetActiveID() == ImGui::GetID("##value") ||
                    (draft.activeId && ImGui::GetActiveID() == draft.activeId))
                    ImGui::ClearActiveID();
                result.push_back({p.id, {EditPhase::cancel, draft.before}});
                draft.active = draft.expressionOpen = false;
                draft.value = draft.before;
            }
            continue;
        }
        auto &draft = s.drafts[p.id];
        const bool cached =
            std::holds_alternative<PropertyArray>(p.value) || std::holds_alternative<PropertyObject>(p.value);
        if (!draft.active && (!cached || !draft.initialized)) {
            draft.value = p.value;
            draft.before = p.value;
            draft.initialized = true;
        }
        ItemScope key(p.id);
        property(tr(p.label.c_str()).c_str(), p.help.empty() ? nullptr : tr(p.help.c_str()).c_str());
        if (options.clipboard && ImGui::IsItemClicked(ImGuiMouseButton_Right))
            ImGui::OpenPopup("##propertyMenu");
        bool changed = false, immediate = false;
        DrawEdit compound;
        bool compoundDrawn = false;
        {
            ScopedDisabled disabled(p.readOnly);
            if (auto *boolValue = std::get_if<bool>(&draft.value)) {
                changed = toggleSwitch("##value", boolValue);
                immediate = true;
            } else if (auto *intValue = std::get_if<std::int64_t>(&draft.value)) {
                const std::int64_t *low = p.bounded ? &p.integerMinimum : nullptr,
                                   *high = p.bounded ? &p.integerMaximum : nullptr;
                changed = ImGui::DragScalar("##value", ImGuiDataType_S64, intValue, 1.f, low, high, "%lld",
                                            ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput);
            } else if (auto *doubleValue = std::get_if<double>(&draft.value)) {
                const double *low = p.bounded ? &p.minimum : nullptr,
                             *high = p.bounded ? &p.maximum : nullptr;
                changed = ImGui::DragScalar("##value", ImGuiDataType_Double, doubleValue, .01f, low, high,
                                            "%.6g", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput);
            } else if (auto *floatValue = std::get_if<float>(&draft.value)) {
                changed = ImGui::DragFloat("##value", floatValue, .01f, p.bounded ? float(p.minimum) : 0.f,
                                           p.bounded ? float(p.maximum) : 0.f, "%.6g",
                                           ImGuiSliderFlags_NoInput |
                                               (p.bounded ? ImGuiSliderFlags_AlwaysClamp : 0));
            } else if (auto *value = std::get_if<std::string>(&draft.value)) {
                if (!p.choices.empty()) {
                    if (ImGui::BeginCombo("##value", tr(value->c_str()).c_str())) {
                        for (const auto &choice : p.choices)
                            if (ImGui::Selectable(trLabel(choice.c_str()).c_str(), choice == *value)) {
                                *value = choice;
                                changed = true;
                            }
                        ImGui::EndCombo();
                    }
                    immediate = true;
                } else
                    changed =
                        detail::inputText("##value", *value, p.password ? ImGuiInputTextFlags_Password : 0,
                                          p.multiline ? ImVec2(-1, dp(84)) : ImVec2{});
            } else {
                std::vector<std::string> path;
                compound = drawValue(draft.value, p, options, s, path, true);
                compoundDrawn = true;
                changed = compound.changed;
                immediate = compound.immediate;
            }
        }
        if (p.readOnly && changed) {
            draft.value = p.value;
            changed = false;
        }
        const bool numeric = std::holds_alternative<std::int64_t>(p.value) ||
                             std::holds_alternative<double>(p.value) ||
                             std::holds_alternative<float>(p.value);
        const bool expressionRequested = numeric && !p.readOnly &&
                                         ((ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) ||
                                          (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter)));
        EditPhase phase = EditPhase::none;
        const bool itemActive = compoundDrawn ? compound.active : ImGui::IsItemActive();
        const bool deactivated = compoundDrawn ? compound.deactivated : ImGui::IsItemDeactivated();
        const bool edited = compoundDrawn ? compound.edited : ImGui::IsItemDeactivatedAfterEdit();
        if (itemActive && ImGui::GetActiveID())
            draft.activeId = ImGui::GetActiveID();
        // Escape cancels whoever ends the edit: this code (still active), ImGui's own drag cancel or
        // keyboard navigation (deactivated this frame). Either way the draft must not commit.
        const bool escaped =
            draft.active && (compound.cancelled ||
                             ((itemActive || deactivated) && ImGui::IsKeyPressed(ImGuiKey_Escape, false)));
        // The frame after ClearActiveID() ImGui reports the deactivation of an edit already cancelled.
        const bool stale = draft.cancelled && !itemActive;
        draft.cancelled = false;
        if (stale) {
        } else if (!p.readOnly && escaped) {
            if (itemActive) {
                ImGui::ClearActiveID();
                draft.cancelled = true;
            }
            phase = EditPhase::cancel;
            draft.value = draft.before;
            draft.active = false;
            draft.error.clear();
        } else if (!p.readOnly) {
            if (compoundDrawn ? compound.activated : ImGui::IsItemActivated()) {
                draft.active = true;
                draft.before = p.value;
                phase = EditPhase::begin;
            }
            if (changed) {
                changed = normalizePropertyValue(p, draft.value, draft.error);
                draft.active = true;
                phase = changed ? EditPhase::update : EditPhase::none;
                if (changed)
                    draft.error.clear();
            }
            if ((immediate && changed) || edited) {
                const bool valid = normalizePropertyValue(p, draft.value, draft.error);
                if (valid && validate)
                    draft.error = validate(p.id, draft.value);
                if (draft.error.empty()) {
                    phase = EditPhase::commit;
                    draft.active = false;
                    draft.initialized = false;
                    s.elements.clear();
                    for (auto &list : s.lists)
                        list.second.activeRow = -1;
                } else
                    phase = EditPhase::none;
            }
            if (deactivated && !itemActive && !edited && draft.active) {
                phase = EditPhase::cancel;
                draft.active = false;
                draft.value = draft.before;
            }
        }
        if (phase != EditPhase::none)
            result.push_back({p.id, {phase, draft.value}});
        bool openExpression = expressionRequested;
        if (options.clipboard && !std::holds_alternative<PropertyArray>(p.value) &&
            ImGui::IsItemClicked(ImGuiMouseButton_Right))
            ImGui::OpenPopup("##propertyMenu");
        if (ImGui::BeginPopup("##propertyMenu")) {
            if (ImGui::MenuItem(trLabel("Copy as JSON").c_str(), nullptr, false, !p.password && !p.mixed))
                try {
                    ImGui::SetClipboardText(propertyToJson(p).c_str());
                } catch (const std::exception &e) {
                    s.clipboardError = e.what();
                }
            if (ImGui::MenuItem(trLabel("Paste value").c_str(), nullptr, false, !p.readOnly)) {
                auto text = ImGui::GetClipboardText();
                auto paste = propertyFromJson(p, text ? text : "", validate);
                s.clipboardError = paste.error;
                if (paste) {
                    result.insert(result.end(), paste.edits.begin(), paste.edits.end());
                    draft.active = false;
                    draft.expressionOpen = false;
                    draft.initialized = false;
                    s.elements.clear();
                    for (auto &list : s.lists)
                        list.second.activeRow = -1;
                }
            }
            if (numeric &&
                ImGui::MenuItem(trLabel("Edit expression...").c_str(), nullptr, false, !p.readOnly))
                openExpression = true;
            if (p.defaultValue &&
                ImGui::MenuItem(trLabel("Reset to default").c_str(), nullptr, false, !p.readOnly)) {
                Property source = p;
                source.value = *p.defaultValue;
                auto paste = propertyFromJson(p, propertyToJson(source), validate);
                s.clipboardError = paste.error;
                if (paste) {
                    result.insert(result.end(), paste.edits.begin(), paste.edits.end());
                    draft.active = draft.expressionOpen = draft.initialized = false;
                    s.elements.clear();
                    for (auto &list : s.lists)
                        list.second.activeRow = -1;
                }
            }
            ImGui::EndPopup();
        }
        if (p.mixed) {
            ImGui::SameLine();
            textFaint("%s", tr("Multiple values").c_str());
        }
        if (openExpression) {
            draft.expressionOpen = draft.expressionFresh = true;
            draft.before = p.value;
            draft.expression = std::visit(
                [](const auto &v) -> std::string {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_integral_v<T>)
                        return std::to_string(v);
                    else if constexpr (std::is_floating_point_v<T>) {
                        std::ostringstream text;
                        text.imbue(std::locale::classic());
                        text << std::setprecision(std::numeric_limits<T>::max_digits10) << v;
                        return text.str();
                    } else
                        return {};
                },
                p.value);
            ImGui::OpenPopup("##expression");
        }
        if (draft.expressionOpen && (p.readOnly || s.revision != revision))
            draft.expressionOpen = false;
        if (ImGui::BeginPopup("##expression")) {
            if (draft.expressionFresh) {
                ImGui::SetKeyboardFocusHere();
                draft.expressionFresh = false;
            }
            textDim("%s", tr("Use Value for the original value").c_str());
            if (detail::inputText("##expressionText", draft.expression,
                                  ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                Property original = p;
                original.value = draft.before;
                PropertyValue value;
                if (!p.readOnly &&
                    evaluatePropertyExpression(original, draft.expression, value, draft.error)) {
                    if (validate)
                        draft.error = validate(p.id, value);
                    if (draft.error.empty()) {
                        result.push_back({p.id, {EditPhase::commit, value}});
                        draft.active = draft.expressionOpen = false;
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape) || !draft.expressionOpen) {
                draft.expressionOpen = false;
                ImGui::CloseCurrentPopup();
            }
            if (!draft.error.empty())
                textColoured(palette().error, "%s", tr(draft.error.c_str()).c_str());
            ImGui::EndPopup();
        }
        if (!draft.error.empty())
            textColoured(palette().error, tr("%s").c_str(), draft.error.c_str());
    }
    endProperties();
    if (!s.clipboardError.empty())
        textColoured(palette().error, "%s", tr(s.clipboardError.c_str()).c_str());
    if (!visible && options.filter && options.filter->active())
        textDim("%s", tr("No matching properties").c_str());
    for (auto it = s.drafts.begin(); it != s.drafts.end();)
        if (!present.count(it->first))
            it = s.drafts.erase(it);
        else
            ++it;
    return result;
}
} // namespace cyui
