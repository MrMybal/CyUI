#include "cyui/modules/Audio.h"

#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace cyui {
namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kArcStart = kPi * 0.75f;
constexpr float kArcSweep = kPi * 1.5f;

float toNormalised(const float value, const float minimum, const float maximum, const bool logarithmic)
{
    if (maximum <= minimum)
        return 0.0f;
    if (logarithmic && minimum > 0.0f)
        return std::clamp(std::log(value / minimum) / std::log(maximum / minimum), 0.0f, 1.0f);
    return std::clamp((value - minimum) / (maximum - minimum), 0.0f, 1.0f);
}

float fromNormalised(const float t, const float minimum, const float maximum, const bool logarithmic)
{
    const auto clamped = std::clamp(t, 0.0f, 1.0f);
    if (logarithmic && minimum > 0.0f)
        return minimum * std::pow(maximum / minimum, clamped);
    return minimum + (maximum - minimum) * clamped;
}

void arc(ImDrawList* list, const ImVec2 centre, const float radius, const float from, const float to, const ImU32 colour,
         const float thickness)
{
    if (std::abs(to - from) < 0.001f)
        return;
    list->PathClear();
    list->PathArcTo(centre, radius, std::min(from, to), std::max(from, to), 32);
    list->PathStroke(colour, 0, thickness);
}

std::string formatValue(const float value, const KnobStyle& style)
{
    char text[64];
    std::snprintf(text, sizeof(text), style.format != nullptr ? style.format : "%.2f",
                  (style.flags & knobInteger) != 0 ? std::round(value) : value);
    std::string out(text);
    if (style.unit != nullptr && style.unit[0] != 0) {
        out += ' ';
        out += style.unit;
    }
    return out;
}

} // namespace

bool knob(const char* id, float* value, const float minimum, const float maximum, const float defaultValue,
          const KnobStyle& style)
{
    const auto& p = palette();
    auto& io = ImGui::GetIO();
    const auto diameter = dp(style.diameter);
    const auto labelHeight = (style.flags & knobNoLabel) != 0 ? 0.0f : ImGui::GetFontSize() * 0.86f;
    const auto width = dp(style.diameter + 12.0f);
    const auto origin = ImGui::GetCursorScreenPos();
    ImGui::PushID(id);
    ImGui::InvisibleButton("##knob", ImVec2(width, diameter + labelHeight));
    const auto knobId = ImGui::GetItemID();
    const auto hovered = ImGui::IsItemHovered();
    const auto active = ImGui::IsItemActive();
    const auto logarithmic = (style.flags & knobLogarithmic) != 0;
    const auto steps = std::max(0, style.steps);
    const auto onStep = [steps](const float position) {
        return steps > 0 ? std::round(position * static_cast<float>(steps)) / static_cast<float>(steps) : position;
    };
    auto changed = false;
    auto t = toNormalised(*value, minimum, maximum, logarithmic);

    // The unrounded position a drag has reached, so small moves add up to a step.
    auto* storage = ImGui::GetStateStorage();
    const auto heldKey = ImGui::GetID("##held");
    const auto movedKey = ImGui::GetID("##moved");
    if (ImGui::IsItemActivated()) {
        storage->SetFloat(heldKey, t);
        storage->SetBool(movedKey, false);
    }
    if (active && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f)) {
        storage->SetBool(movedKey, true);
        const auto speed = 0.0055f * valueSpeed() / std::max(0.5f, ImGui::GetStyle().FontScaleDpi);
        const auto held = std::clamp(storage->GetFloat(heldKey, t) + (-io.MouseDelta.y + io.MouseDelta.x * 0.35f) * speed, 0.0f, 1.0f);
        storage->SetFloat(heldKey, held);
        t = onStep(held);
        changed = true;
    }
    if (hovered && io.MouseWheel != 0.0f) {
        const auto notch = steps > 0 ? 1.0f / static_cast<float>(steps) : 0.02f * valueSpeed();
        t = onStep(std::clamp(t + io.MouseWheel * notch, 0.0f, 1.0f));
        changed = true;
        takeWheel();
    }
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        t = toNormalised(defaultValue, minimum, maximum, logarithmic);
        storage->SetFloat(heldKey, t);
        changed = true;
    }
    // Its number clicked without a drag: typed in.
    const auto onNumber = labelHeight > 0.0f && io.MouseClickedPos[0].y >= origin.y + diameter;
    const auto openEntry = ImGui::IsItemDeactivated() && !storage->GetBool(movedKey, false) && onNumber
                           && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    auto typedIn = false;
    if (labelHeight > 0.0f) {
        auto typed = static_cast<double>(*value);
        if (valueEntry(knobId, ImVec2(origin.x, origin.y + diameter), ImVec2(origin.x + width, origin.y + diameter + labelHeight),
                       typed, minimum, maximum, openEntry)) {
            *value = (style.flags & knobInteger) != 0 ? std::round(static_cast<float>(typed)) : static_cast<float>(typed);
            t = toNormalised(*value, minimum, maximum, logarithmic);
            typedIn = true;
        }
    }
    if (changed) {
        auto next = fromNormalised(t, minimum, maximum, logarithmic);
        if ((style.flags & knobInteger) != 0)
            next = std::round(next);
        changed = next != *value;
        *value = next;
        t = toNormalised(*value, minimum, maximum, logarithmic);
    }
    changed = changed || typedIn;

    auto* list = ImGui::GetWindowDrawList();
    const auto colour = style.colour != 0 ? style.colour : p.accent;
    const ImVec2 centre(origin.x + width * 0.5f, origin.y + diameter * 0.5f);
    const auto radius = diameter * 0.5f - dp(2.0f);
    const auto thickness = std::max(2.0f, diameter * 0.095f);
    arc(list, centre, radius, kArcStart, kArcStart + kArcSweep, p.bg5, thickness);
    const auto from = (style.flags & knobBipolar) != 0 ? kArcStart + kArcSweep * 0.5f : kArcStart;
    arc(list, centre, radius, from, kArcStart + kArcSweep * t, active || hovered ? brighter(colour, 0.15f) : colour, thickness);
    list->AddCircleFilled(centre, radius - thickness * 0.9f, hovered || active ? p.bg5 : p.bg4, 24);
    const auto angle = kArcStart + kArcSweep * t;
    const ImVec2 tip(centre.x + std::cos(angle) * (radius - thickness * 1.1f), centre.y + std::sin(angle) * (radius - thickness * 1.1f));
    const ImVec2 inner(centre.x + std::cos(angle) * radius * 0.25f, centre.y + std::sin(angle) * radius * 0.25f);
    list->AddLine(inner, tip, p.text, std::max(1.5f, thickness * 0.55f));
    if (style.modulation >= 0.0f) {
        const auto m = kArcStart + kArcSweep * std::clamp(style.modulation, 0.0f, 1.0f);
        list->AddCircleFilled(ImVec2(centre.x + std::cos(m) * radius, centre.y + std::sin(m) * radius), thickness * 0.7f, p.info, 12);
    }
    if (labelHeight > 0.0f) {
        const auto showValue = hovered || active;
        const auto text = showValue ? formatValue(*value, style) : std::string(style.label != nullptr ? style.label : "");
        ScopedFont font(nullptr, 0.82f);
        const auto room = width + dp(12.0f);
        const auto size = ImGui::CalcTextSize(text.c_str());
        clippedText(list, ImVec2(origin.x + (width - std::min(size.x, room)) * 0.5f, origin.y + diameter + dp(1.0f)), room,
                    showValue ? p.text : p.textDim, text.c_str());
    } else if (hovered || active) {
        ImGui::SetTooltip("%s", formatValue(*value, style).c_str());
    }
    ImGui::PopID();
    return changed;
}

float meterPosition(const float gainDb)
{
    if (gainDb <= -70.0f)
        return 0.0f;
    return std::clamp(std::exp((gainDb - 6.0f) / 26.0f), 0.0f, 1.0f);
}

bool fader(const char* id, float* gainDb, const float minimumDb, const float maximumDb, const ImVec2 size, const ImU32 colour)
{
    const auto& p = palette();
    auto& io = ImGui::GetIO();
    const auto origin = ImGui::GetCursorScreenPos();
    ImGui::PushID(id);
    ImGui::InvisibleButton("##fader", size);
    const auto hovered = ImGui::IsItemHovered();
    const auto active = ImGui::IsItemActive();
    auto changed = false;
    const auto travel = size.y - dp(14.0f);
    auto position = meterPosition(*gainDb);
    if (active && io.MouseDelta.y != 0.0f) {
        position = std::clamp(position - io.MouseDelta.y / travel * valueSpeed(), 0.0f, 1.0f);
        *gainDb = position <= 0.0f ? minimumDb : std::clamp(6.0f + 26.0f * std::log(position), minimumDb, maximumDb);
        changed = true;
    }
    if (hovered && io.MouseWheel != 0.0f) {
        takeWheel();
        *gainDb = std::clamp(*gainDb + io.MouseWheel * valueSpeed(), minimumDb, maximumDb);
        changed = true;
    }
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        *gainDb = std::clamp(0.0f, minimumDb, maximumDb);
        changed = true;
    }
    auto* list = ImGui::GetWindowDrawList();
    const auto centreX = origin.x + size.x * 0.5f;
    const auto top = origin.y + dp(7.0f);
    const auto bottom = top + travel;
    for (const auto tick : { 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -48.0f }) {
        const auto y = bottom - meterPosition(tick) * travel;
        list->AddLine(ImVec2(origin.x, y), ImVec2(origin.x + dp(4.0f), y), p.textFaint);
    }
    list->AddRectFilled(ImVec2(centreX - dp(2.0f), top), ImVec2(centreX + dp(2.0f), bottom), p.bg0, dp(2.0f));
    const auto handleY = bottom - meterPosition(*gainDb) * travel;
    const auto fill = colour != 0 ? colour : p.accent;
    list->AddRectFilled(ImVec2(centreX - dp(2.0f), handleY), ImVec2(centreX + dp(2.0f), bottom), alpha(fill, 0.55f), dp(2.0f));
    const auto handleWidth = std::min(size.x - dp(4.0f), dp(22.0f));
    const ImVec2 handleMin(centreX - handleWidth * 0.5f, handleY - dp(6.0f));
    const ImVec2 handleMax(centreX + handleWidth * 0.5f, handleY + dp(6.0f));
    list->AddRectFilled(handleMin, handleMax, hovered || active ? brighter(p.bg5, 0.25f) : p.bg5, dp(3.0f));
    list->AddLine(ImVec2(handleMin.x + dp(3.0f), handleY), ImVec2(handleMax.x - dp(3.0f), handleY), p.text, 1.5f);
    if (hovered || active)
        ImGui::SetTooltip("%.1f dB", *gainDb);
    ImGui::PopID();
    return changed;
}

void meter(const char* id, const ImVec2 size, const float peakLeft, const float peakRight, const float rmsLeft,
           const float rmsRight, const bool vertical)
{
    const auto& p = palette();
    const auto origin = ImGui::GetCursorScreenPos();
    ImGui::PushID(id);
    ImGui::Dummy(size);
    auto* storage = ImGui::GetStateStorage();
    const auto holdId = ImGui::GetID("hold");
    const auto holdTimeId = ImGui::GetID("holdTime");
    auto* list = ImGui::GetWindowDrawList();
    const auto now = static_cast<float>(ImGui::GetTime());
    const auto toDb = [](const float gain) { return gain > 1.0e-5f ? 20.0f * std::log10(gain) : -100.0f; };
    const auto peakDb = std::max(toDb(peakLeft), toDb(peakRight));
    auto hold = storage->GetFloat(holdId, -100.0f);
    auto holdTime = storage->GetFloat(holdTimeId, 0.0f);
    if (peakDb >= hold || now - holdTime > 1.5f) {
        hold = peakDb;
        holdTime = now;
    }
    storage->SetFloat(holdId, hold);
    storage->SetFloat(holdTimeId, holdTime);

    list->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), p.bg0, dp(2.0f));
    const auto gap = dp(1.0f);
    const auto low = p.success;
    const auto midColour = p.warning;
    const auto high = p.error;
    const auto drawBar = [&](const int index, const float peak, const float rms) {
        const auto segment = [&](const float from, const float to, const ImU32 colour) {
            if (vertical) {
                const auto barWidth = (size.x - gap) * 0.5f;
                const auto x0 = origin.x + static_cast<float>(index) * (barWidth + gap);
                list->AddRectFilled(ImVec2(x0, origin.y + size.y * (1.0f - to)), ImVec2(x0 + barWidth, origin.y + size.y * (1.0f - from)),
                                    colour);
            } else {
                const auto barHeight = (size.y - gap) * 0.5f;
                const auto y0 = origin.y + static_cast<float>(index) * (barHeight + gap);
                list->AddRectFilled(ImVec2(origin.x + size.x * from, y0), ImVec2(origin.x + size.x * to, y0 + barHeight), colour);
            }
        };
        const auto mid = meterPosition(-12.0f);
        const auto top = meterPosition(-3.0f);
        const auto draw = [&](const float level, const float alphaValue) {
            segment(0.0f, std::min(level, mid), alpha(low, alphaValue));
            if (level > mid)
                segment(mid, std::min(level, top), alpha(midColour, alphaValue));
            if (level > top)
                segment(top, level, alpha(high, alphaValue));
        };
        draw(meterPosition(toDb(peak)), 0.45f);
        draw(meterPosition(toDb(rms)), 1.0f);
    };
    drawBar(0, peakLeft, rmsLeft);
    drawBar(1, peakRight, rmsRight);
    const auto holdPos = meterPosition(hold);
    if (holdPos > 0.0f) {
        const auto colour = alpha(hold > -0.1f ? high : p.text, 0.8f);
        if (vertical) {
            const auto y = origin.y + size.y * (1.0f - holdPos);
            list->AddLine(ImVec2(origin.x, y), ImVec2(origin.x + size.x, y), colour, 1.0f);
        } else {
            const auto x = origin.x + size.x * holdPos;
            list->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + size.y), colour, 1.0f);
        }
    }
    ImGui::PopID();
}

} // namespace cyui
