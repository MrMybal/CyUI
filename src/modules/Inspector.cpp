#include "cyui/modules/Inspector.h"
#include "ModuleInternal.h"
#include <unordered_set>

namespace cyui {
std::vector<PropertyEdit> inspector(const char *id, InspectorState &s, ArrayView<Property> properties,
                                    std::uint64_t revision, const PropertyValidator &validate) {
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
    for (std::size_t i = 0; i < properties.count; ++i) {
        const auto &p = properties[i];
        present.insert(p.id);
        auto &draft = s.drafts[p.id];
        if (!draft.active) {
            draft.value = p.value;
            draft.before = p.value;
        }
        ItemScope key(p.id);
        property(p.label.c_str(), p.help.c_str());
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
                                            ImGuiSliderFlags_AlwaysClamp);
            } else if (auto *doubleValue = std::get_if<double>(&draft.value)) {
                const double *low = p.bounded ? &p.minimum : nullptr,
                             *high = p.bounded ? &p.maximum : nullptr;
                changed = ImGui::DragScalar("##value", ImGuiDataType_Double, doubleValue, .01f, low, high,
                                            "%.6g", ImGuiSliderFlags_AlwaysClamp);
            } else if (auto *value = std::get_if<std::string>(&draft.value)) {
                if (!p.choices.empty()) {
                    if (ImGui::BeginCombo("##value", value->c_str())) {
                        for (const auto &choice : p.choices)
                            if (ImGui::Selectable(choice.c_str(), choice == *value)) {
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
            }
        }
        EditPhase phase = EditPhase::none;
        if (!p.readOnly) {
            if (ImGui::IsItemActivated()) {
                draft.active = true;
                draft.before = p.value;
                phase = EditPhase::begin;
            }
            if (changed) {
                draft.active = true;
                phase = EditPhase::update;
                draft.error.clear();
            }
            if ((immediate && changed) || ImGui::IsItemDeactivatedAfterEdit()) {
                draft.error = validate ? validate(p.id, draft.value) : std::string{};
                if (draft.error.empty()) {
                    phase = EditPhase::commit;
                    draft.active = false;
                } else
                    phase = EditPhase::none;
            }
            if (draft.active && ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                phase = EditPhase::cancel;
                draft.value = draft.before;
                draft.active = false;
                draft.error.clear();
            }
            if (ImGui::IsItemDeactivated() && !ImGui::IsItemDeactivatedAfterEdit() && draft.active) {
                phase = EditPhase::cancel;
                draft.active = false;
                draft.value = draft.before;
            }
        }
        if (phase != EditPhase::none)
            result.push_back({p.id, {phase, draft.value}});
        if (!draft.error.empty())
            textColoured(palette().error, "%s", draft.error.c_str());
    }
    endProperties();
    for (auto it = s.drafts.begin(); it != s.drafts.end();)
        if (!present.count(it->first))
            it = s.drafts.erase(it);
        else
            ++it;
    return result;
}
} // namespace cyui
