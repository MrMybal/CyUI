#include "cyui/modules/Inspector.h"
#include "ModuleInternal.h"
#include <unordered_set>
#include <cfloat>
#include <sstream>
#include <iomanip>
#include <locale>

namespace cyui {
namespace {
bool compoundNumber(double &number, const Property &field, bool &immediate) {
    const double *low = field.bounded ? &field.minimum : nullptr;
    const double *high = field.bounded ? &field.maximum : nullptr;
    bool changed = ImGui::DragScalar("##value", ImGuiDataType_Double, &number, .01f, low, high, "%.6g",
                                     ImGuiSliderFlags_NoInput | ImGuiSliderFlags_AlwaysClamp);
    const auto id = ImGui::GetItemID();
    const auto min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
    if (valueEntry(id, min, max, number, field.bounded ? field.minimum : -DBL_MAX,
                   field.bounded ? field.maximum : DBL_MAX,
                   ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))) {
        changed = immediate = true;
        markEdited(id);
    }
    return changed;
}
bool drawCompound(PropertyValue &value, const Property &field, const InspectorOptions &options, ItemId id,
                  bool &immediate) {
    bool changed = false;
    ImGui::BeginGroup();
    if (auto v = std::get_if<PropertyVector>(&value)) {
        if (v->values.size() >= 2 && v->values.size() <= 4) {
            const float width = (ImGui::CalcItemWidth() -
                                 ImGui::GetStyle().ItemInnerSpacing.x * float(v->values.size() - 1)) /
                                float(v->values.size());
            for (std::size_t i = 0; i < v->values.size(); ++i) {
                ScopedID component{int(i)};
                if (i)
                    ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
                ImGui::SetNextItemWidth(width);
                changed |= compoundNumber(v->values[i], field, immediate);
            }
        }
    } else if (auto c = std::get_if<PropertyColor>(&value))
        changed = ImGui::ColorEdit4("##value", c->rgba.data());
    else if (auto a = std::get_if<PropertyAsset>(&value)) {
        ImGui::SetNextItemWidth(-dp(34));
        changed = detail::inputText("##value", a->path);
        ImGui::SameLine();
        if (button("...") && options.browseAsset)
            options.browseAsset(id);
    } else if (auto list = std::get_if<PropertyArray>(&value)) {
        for (std::size_t i = 0; i < list->values.size(); ++i) {
            ScopedID row{int(i)};
            ImGui::SetNextItemWidth(-dp(34));
            auto atom = std::visit([](const auto &a) -> PropertyValue { return a; }, list->values[i]);
            bool itemChanged = false;
            if (auto b = std::get_if<bool>(&atom))
                itemChanged = ImGui::Checkbox("##value", b);
            else if (auto integer = std::get_if<std::int64_t>(&atom))
                itemChanged =
                    ImGui::DragScalar("##value", ImGuiDataType_S64, integer, 1, nullptr, nullptr, "%lld");
            else if (auto number = std::get_if<double>(&atom))
                itemChanged = compoundNumber(*number, field, immediate);
            else if (auto single = std::get_if<float>(&atom))
                itemChanged = ImGui::DragFloat("##value", single, .01f);
            else if (auto s = std::get_if<std::string>(&atom))
                itemChanged = detail::inputText("##value", *s);
            else
                itemChanged = drawCompound(atom, field, options, id, immediate);
            if (itemChanged) {
                std::visit(
                    [&](const auto &a) {
                        using T = std::decay_t<decltype(a)>;
                        if constexpr (!std::is_same_v<T, PropertyArray>)
                            list->values[i] = a;
                    },
                    atom);
                changed = true;
            }
            ImGui::SameLine();
            if (ghostButton("-")) {
                list->values.erase(list->values.begin() + std::ptrdiff_t(i));
                changed = immediate = true;
                break;
            }
        }
        if (list->values.size() < 1024 && ghostButton(trLabel("Add element").c_str())) {
            list->values.push_back(list->values.empty() ? PropertyAtom{std::string{}} : list->values.back());
            changed = immediate = true;
        }
    }
    ImGui::EndGroup();
    return changed;
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
    std::unordered_set<ItemId> present;
    if (s.revision != revision) {
        for (auto &entry : s.drafts)
            if (entry.second.active)
                result.push_back({entry.first, {EditPhase::cancel, entry.second.before}});
        s.drafts.clear();
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
                if (ImGui::GetActiveID() == ImGui::GetID("##value"))
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
                if (ImGui::GetActiveID() == ImGui::GetID("##value"))
                    ImGui::ClearActiveID();
                result.push_back({p.id, {EditPhase::cancel, draft.before}});
                draft.active = draft.expressionOpen = false;
                draft.value = draft.before;
            }
            continue;
        }
        auto &draft = s.drafts[p.id];
        if (!draft.active) {
            draft.value = p.value;
            draft.before = p.value;
        }
        ItemScope key(p.id);
        property(tr(p.label.c_str()).c_str(), p.help.empty() ? nullptr : tr(p.help.c_str()).c_str());
        if (options.clipboard && ImGui::IsItemClicked(ImGuiMouseButton_Right))
            ImGui::OpenPopup("##propertyMenu");
        bool changed = false, immediate = false;
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
            } else
                changed = drawCompound(draft.value, p, options, p.id, immediate);
        }
        const bool numeric = std::holds_alternative<std::int64_t>(p.value) ||
                             std::holds_alternative<double>(p.value) ||
                             std::holds_alternative<float>(p.value);
        const bool expressionRequested = numeric && !p.readOnly &&
                                         ((ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) ||
                                          (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter)));
        EditPhase phase = EditPhase::none;
        const bool itemActive = ImGui::IsItemActive(), deactivated = ImGui::IsItemDeactivated();
        // Escape cancels whoever ends the edit: this code (still active), ImGui's own drag cancel or
        // keyboard navigation (deactivated this frame). Either way the draft must not commit.
        const bool escaped =
            draft.active && (itemActive || deactivated) && ImGui::IsKeyPressed(ImGuiKey_Escape, false);
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
            if (ImGui::IsItemActivated()) {
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
            if ((immediate && changed) || ImGui::IsItemDeactivatedAfterEdit()) {
                const bool valid = normalizePropertyValue(p, draft.value, draft.error);
                if (valid && validate)
                    draft.error = validate(p.id, draft.value);
                if (draft.error.empty()) {
                    phase = EditPhase::commit;
                    draft.active = false;
                } else
                    phase = EditPhase::none;
            }
            if (deactivated && !ImGui::IsItemDeactivatedAfterEdit() && draft.active) {
                phase = EditPhase::cancel;
                draft.active = false;
                draft.value = draft.before;
            }
        }
        if (phase != EditPhase::none)
            result.push_back({p.id, {phase, draft.value}});
        bool openExpression = expressionRequested;
        if (options.clipboard && ImGui::IsItemClicked(ImGuiMouseButton_Right))
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
                if (paste)
                    result.insert(result.end(), paste.edits.begin(), paste.edits.end());
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
