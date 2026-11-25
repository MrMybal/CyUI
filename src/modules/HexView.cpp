#include "cyui/modules/HexView.h"
#include "cyui/CoreWidgets.h"
#include "ModuleInternal.h"
#include <charconv>
#include <cstdio>

namespace cyui {
namespace {
bool contains(HexRange r, std::uint64_t offset) {
    return offset >= r.offset && offset - r.offset < r.length;
}

std::optional<HexRange> selected(const HexViewState &state) {
    if (!state.anchor || !state.caret) return {};
    const auto first = std::min(*state.anchor, *state.caret);
    return HexRange{first, std::max(*state.anchor, *state.caret) - first + 1};
}

void copy(HexRange range, const HexSource &source, std::size_t limit, bool ascii) {
    const auto count = std::size_t(std::min<std::uint64_t>(range.length, std::min<std::size_t>(limit, 1024 * 1024)));
    if (!count || !source.read) return;
    std::vector<std::uint8_t> bytes(count);
    const auto read = std::min(count, source.read(range.offset, bytes.data(), count));
    if (!read) return;
    constexpr char digits[] = "0123456789ABCDEF";
    std::string text;
    text.reserve(read * (ascii ? 1 : 3));
    for (std::size_t i = 0; i < read; ++i) {
        const auto value = bytes[i];
        if (ascii) text.push_back(value >= 32 && value < 127 ? char(value) : '.');
        else {
            if (i) text.push_back(' ');
            text.push_back(digits[value >> 4]);
            text.push_back(digits[value & 15]);
        }
    }
    ImGui::SetClipboardText(text.c_str());
}
} // namespace

HexViewResult hexView(const char *id, HexViewState &state, const HexSource &source, ImVec2 size,
                      const HexViewOptions &options) {
    ScopedID scope(id);
    HexViewResult result;
    if (source.size == 0) {
        result.selectionChanged = state.anchor.has_value();
        state.anchor.reset();
        state.caret.reset();
        state.topOffset = 0;
        state.selecting = state.scrollbar = false;
        emptyState(tr("No bytes to display").c_str(), "");
        return result;
    }
    const auto finalByte = source.size - 1;
    const auto previousAnchor = state.anchor, previousCaret = state.caret;
    if (state.anchor) *state.anchor = std::min(*state.anchor, finalByte);
    if (state.caret) *state.caret = std::min(*state.caret, finalByte);
    ImGui::SetNextItemWidth(dp(126));
    const auto hint = tr("Hex offset");
    ImGui::PushFont(monoFont(), 0);
    auto &input = state.offsetInput;
    const bool go = ImGui::InputTextWithHint("##offset", hint.c_str(), input.data(), input.capacity() + 1,
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackResize, detail::resizeText, &input);
    ImGui::PopFont();
    ImGui::SameLine();
    if (button(trLabel("Go to offset").c_str()) || go) {
        std::string_view digits(input);
        if (digits.size() >= 2 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) digits.remove_prefix(2);
        std::uint64_t offset = 0;
        const auto parse = std::from_chars(digits.data(), digits.data() + digits.size(), offset, 16);
        if (parse.ec == std::errc{} && parse.ptr == digits.data() + digits.size()) {
            state.anchor = state.caret = std::min(offset, finalByte);
            state.reveal = state.caret;
        }
    }
    if (auto selection = selected(state)) {
        ImGui::SameLine();
        textDim("0x%llX  ·  %llu B", static_cast<unsigned long long>(selection->offset),
                static_cast<unsigned long long>(selection->length));
    }

    ImGui::PushFont(monoFont(), 0);
    const auto room = ImGui::GetContentRegionAvail();
    const auto pos = ImGui::GetCursorScreenPos();
    const float line = ImGui::GetTextLineHeightWithSpacing(), cell = ImGui::CalcTextSize("0").x;
    const float padding = dp(8), rail = dp(10);
    size = {std::max(dp(40), size.x > 0 ? size.x : room.x + size.x),
            std::max(line * 3 + padding * 2, size.y > 0 ? size.y : room.y + size.y)};
    const ImVec2 end{pos.x + size.x, pos.y + size.y};
    const int desired = std::clamp(options.bytesPerRow, 1, 32);
    const auto perRow = std::uint64_t(std::clamp(int((size.x - padding * 4 - rail - cell * 17) / (cell * 4)), 1, desired));
    const auto rows = finalByte / perRow + 1;
    const auto visible = std::min(rows, std::uint64_t(std::min(512.f, std::max(1.f, std::floor((size.y - padding * 2) / line) - 1.f))));
    const auto lastTop = rows - visible;
    auto top = std::min(state.topOffset / perRow, lastTop);
    const auto reveal = [&](std::uint64_t byte) {
        const auto row = std::min(byte, finalByte) / perRow;
        if (row < top) top = row;
        if (row - top >= visible) top = std::min(lastTop, row - visible + 1);
    };
    if (state.reveal) { reveal(*state.reveal); state.reveal.reset(); }
    ImGui::InvisibleButton("##bytes", size, ImGuiButtonFlags_EnableNav);
    const bool hovered = ImGui::IsItemHovered();
    const bool focus = ImGui::IsItemFocused() || ImGui::IsItemActive();
    const auto &io = ImGui::GetIO();
    const float scrollX = end.x - padding - rail;
    const float y0 = pos.y + padding + line;
    const float body = std::max(line, end.y - padding - y0);
    const float hexX = pos.x + padding * 2 + cell * 16;
    const float asciiX = hexX + cell * 3 * float(perRow) + padding;
    const auto shift = [&](int amount) {
        if (amount > 0) top += std::min(lastTop - top, std::uint64_t(amount));
        if (amount < 0) top -= std::min(top, std::uint64_t(-amount));
    };
    if (float wheel = itemWheel()) shift(int(std::clamp(-wheel * 3, -512.f, 512.f)));
    const bool pressed = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    if (pressed && io.MousePos.x >= scrollX && lastTop) state.scrollbar = true;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || io.AppFocusLost) state.scrollbar = state.selecting = false;
    if (state.scrollbar) {
        const auto fraction = std::clamp((io.MousePos.y - y0) / body, 0.f, 1.f);
        top = fraction >= 1.f ? lastTop : std::min(lastTop, std::uint64_t(double(lastTop) * fraction));
    }
    const auto mouseByte = [&](bool clamp) -> std::optional<std::uint64_t> {
        if (!clamp && (io.MousePos.y < y0 || io.MousePos.y >= y0 + line * float(visible) ||
                       io.MousePos.x < hexX || io.MousePos.x >= std::min(scrollX, asciiX + cell * float(perRow)))) return {};
        const auto row = std::uint64_t(std::clamp(std::floor((io.MousePos.y - y0) / line), 0.f, float(visible - 1)));
        const bool ascii = io.MousePos.x >= asciiX;
        const auto column = std::uint64_t(std::clamp(std::floor((io.MousePos.x - (ascii ? asciiX : hexX)) /
                                                              (cell * (ascii ? 1 : 3))), 0.f, float(perRow - 1)));
        const auto start = (top + row) * perRow;
        return column > finalByte - start && !clamp ? std::nullopt : std::optional{start + std::min(column, finalByte - start)};
    };
    if (hovered) result.hovered = mouseByte(false);
    if (pressed && result.hovered && !state.scrollbar) {
        if (!io.KeyShift || !state.anchor) state.anchor = result.hovered;
        state.caret = result.hovered;
        state.selecting = true;
    }
    if (state.selecting) {
        if (io.MousePos.y < y0 && top) shift(-1);
        if (io.MousePos.y >= y0 + body) shift(1);
        state.caret = mouseByte(true);
    }
    if (focus && !state.selecting) {
        const auto move = [&](bool forward, std::uint64_t step) {
            const auto from = state.caret.value_or(top * perRow);
            const auto to = forward ? from + std::min(step, finalByte - from) : from - std::min(step, from);
            if (!io.KeyShift || !state.anchor) state.anchor = to;
            state.caret = to;
            reveal(to);
        };
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) move(false, 1);
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) move(true, 1);
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) move(false, perRow);
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) move(true, perRow);
        if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) move(false, perRow * visible);
        if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) move(true, perRow * visible);
    }
    state.topOffset = top * perRow;
    result.selection = selected(state);
    result.selectionChanged = state.anchor != previousAnchor || state.caret != previousCaret;
    // Dear ImGui routes the platform's primary modifier to Ctrl, including Command on macOS.
    if (focus && result.selection && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_C))
        copy(*result.selection, source, options.clipboardBytes, false);
    if (ImGui::BeginPopupContextItem("##hex-menu")) {
        if (ImGui::MenuItem(trLabel("Copy as hex").c_str(), nullptr, false, result.selection.has_value()))
            copy(*result.selection, source, options.clipboardBytes, false);
        if (ImGui::MenuItem(trLabel("Copy as ASCII").c_str(), nullptr, false, result.selection.has_value()))
            copy(*result.selection, source, options.clipboardBytes, true);
        ImGui::EndPopup();
    }

    const auto count = std::size_t(std::min<std::uint64_t>(visible * perRow, source.size - state.topOffset));
    std::vector<std::uint8_t> bytes(count);
    const auto read = source.read ? std::min(count, source.read(state.topOffset, bytes.data(), count)) : 0;
    auto *drawing = ImGui::GetWindowDrawList();
    const auto &p = palette();
    drawing->AddRectFilled(pos, end, p.bg0, ImGui::GetStyle().FrameRounding);
    drawing->PushClipRect({pos.x + padding, pos.y + padding}, {scrollX, end.y - padding}, true);
    drawing->AddText({pos.x + padding, pos.y + padding}, p.textDim, tr("Offset").c_str());
    drawing->AddText({hexX, pos.y + padding}, p.textDim, "HEX");
    drawing->AddText({asciiX, pos.y + padding}, p.textDim, "ASCII");
    drawing->AddLine({pos.x + padding, y0 - dp(2)}, {scrollX, y0 - dp(2)}, p.line, dp(1));
    std::vector<std::size_t> highlights;
    for (std::size_t n = 0; n < options.highlights.count; ++n) {
        const auto range = options.highlights[n].range;
        if (range.length && range.offset <= state.topOffset + (count - 1) &&
            (range.offset >= state.topOffset || contains(range, state.topOffset))) highlights.push_back(n);
    }
    for (std::size_t i = 0; i < count; ++i) {
        const auto offset = state.topOffset + i;
        const auto column = float(i % perRow);
        const float y = y0 + float(i / perRow) * line;
        const float x = hexX + column * cell * 3, a = asciiX + column * cell;
        if (i % perRow == 0) {
            char address[17];
            std::snprintf(address, sizeof(address), "%016llX", static_cast<unsigned long long>(offset));
            drawing->AddText({pos.x + padding, y}, p.textDim, address);
        }
        ImU32 background = 0;
        for (auto n : highlights) {
            const auto &mark = options.highlights[n];
            if (!contains(mark.range, offset)) continue;
            background = alpha(categorical(mark.colorIndex), .24f);
            if (result.hovered == offset && !mark.label.empty() && !state.selecting) ImGui::SetTooltip("%s", mark.label.c_str());
            break;
        }
        if (result.selection && contains(*result.selection, offset)) background = p.selection;
        if (background) {
            drawing->AddRectFilled({x, y}, {x + cell * 3, y + line}, background);
            drawing->AddRectFilled({a, y}, {a + cell, y + line}, background);
        }
        const unsigned value = bytes[i];
        char hex[3] = "??";
        if (i < read) std::snprintf(hex, sizeof(hex), "%02X", value);
        char ascii[2]{i < read && value >= 32 && value < 127 ? char(value) : '.', '\0'};
        drawing->AddText({x, y}, i < read ? p.text : p.textFaint, hex);
        drawing->AddText({a, y}, p.textDim, ascii);
    }
    drawing->PopClipRect();
    if (lastTop) {
        const float thumb = std::min(body, std::max(dp(14), body * (float(visible) / float(rows))));
        const float y = y0 + (body - thumb) * float(double(top) / double(lastTop));
        drawing->AddRectFilled({scrollX, y0}, {scrollX + rail, y0 + body}, p.bg2, rail * .5f);
        const auto marker = [&](HexRange range, ImU32 ink) {
            if (!range.length || range.offset > finalByte) return;
            const auto endByte = range.offset + std::min(range.length - 1, finalByte - range.offset);
            const float a = y0 + body * float(double(range.offset) / double(source.size));
            const float b = y0 + body * float(double(endByte) / double(source.size));
            drawing->AddRectFilled({scrollX, std::min(a, y0 + body - dp(1))},
                                  {scrollX + rail, std::min(std::max(a + dp(1), b), y0 + body)}, ink);
        };
        for (std::size_t n = 0; n < options.highlights.count; ++n)
            marker(options.highlights[n].range, alpha(categorical(options.highlights[n].colorIndex), .55f));
        if (result.selection) marker(*result.selection, p.accentDim);
        drawing->AddRectFilled({scrollX, y}, {scrollX + rail, y + thumb}, state.scrollbar ? p.accent : p.bg5, rail * .5f);
    }
    ImGui::PopFont();
    return result;
}

} // namespace cyui
