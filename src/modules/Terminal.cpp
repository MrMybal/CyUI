#include "cyui/modules/Terminal.h"
#include <imgui_internal.h>
#include <vterm.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <limits>
#include <new>
#include <utility>
#include <vector>

namespace cyui {
namespace {

TerminalSize bounded(TerminalSize size) {
    return {std::clamp(size.columns, 2, 300), std::clamp(size.rows, 2, 150)};
}

void appendUtf8(std::string &out, unsigned codepoint) {
    char bytes[5];
    out.append(bytes, ImTextCharToUtf8(bytes, codepoint));
}

bool rightHalf(const VTermScreenCell &cell) { return cell.chars[0] == UINT32_MAX; }

void appendCell(std::string &out, const VTermScreenCell &cell) {
    if (rightHalf(cell)) return;
    if (!cell.chars[0] || cell.attrs.conceal) {
        out.push_back(' ');
        if (cell.width == 2) out.push_back(' ');
        return;
    }
    for (unsigned cp : cell.chars) {
        if (!cp) break;
        appendUtf8(out, cp);
    }
}

struct VTermDeleter {
    void operator()(VTerm *term) const { vterm_free(term); }
};

using Colors = std::array<ImU32, 16>;

ImU32 displayColor(VTermScreen *screen, VTermColor color, const Colors &colors) {
    if (VTERM_COLOR_IS_DEFAULT_FG(&color)) return palette().text;
    if (VTERM_COLOR_IS_DEFAULT_BG(&color)) return palette().bg0;
    if (VTERM_COLOR_IS_INDEXED(&color) && color.index < colors.size())
        return colors[color.index];
    vterm_screen_convert_color_to_rgb(screen, &color);
    // Map application-supplied colors onto the current theme.
    ImU32 nearest = colors.front();
    int distance = std::numeric_limits<int>::max();
    for (auto candidate : colors) {
        const int r = int((candidate >> IM_COL32_R_SHIFT) & 255) - color.red;
        const int g = int((candidate >> IM_COL32_G_SHIFT) & 255) - color.green;
        const int b = int((candidate >> IM_COL32_B_SHIFT) & 255) - color.blue;
        const int difference = r*r + g*g + b*b;
        if (difference < distance) {
            distance = difference;
            nearest = candidate;
        }
    }
    return nearest;
}

float luminance(ImU32 color) {
    static const auto linear = [] {
        std::array<float, 256> values{};
        for (unsigned i = 0; i < values.size(); ++i) {
            const float s = i / 255.f;
            values[i] = s <= .04045f ? s / 12.92f : std::pow((s + .055f) / 1.055f, 2.4f);
        }
        return values;
    }();
    return .2126f * linear[(color >> IM_COL32_R_SHIFT) & 255] +
           .7152f * linear[(color >> IM_COL32_G_SHIFT) & 255] +
           .0722f * linear[(color >> IM_COL32_B_SHIFT) & 255];
}

ImU32 contrastInk(ImU32 ink, ImU32 background, const Palette &p) {
    const float ground = luminance(background) + .05f;
    const auto contrast = [ground](ImU32 color) {
        const float light = luminance(color) + .05f;
        return std::max(light, ground) / std::min(light, ground);
    };
    if (contrast(ink) >= 4.5f) return ink;
    const auto readable = contrast(p.text) >= contrast(p.bg0) ? p.text : p.bg0;
    for (int step = 1; step < 5; ++step) {
        const auto shade = mix(ink, readable, step * .2f);
        if (contrast(shade) >= 4.5f) return shade;
    }
    return readable;
}

constexpr std::size_t inputLimit = 512 * 1024;

} // namespace

struct Terminal::Model {
    struct Line {
        std::vector<VTermScreenCell> cells;
        bool continued;
    };

    Model(TerminalSize size, unsigned limit) : grid(bounded(size)), historyLimit(limit),
        term(vterm_new(grid.rows, grid.columns)) {
        if (!term) throw std::bad_alloc();
        vterm_set_utf8(term.get(), 1);
        vterm_output_set_callback(term.get(), [](const char *bytes, size_t n, void *user) {
            auto &output = static_cast<Model *>(user)->input;
            if (n <= inputLimit - output.size()) output.append(bytes, n);
        }, this);
        screen = vterm_obtain_screen(term.get());
        state = vterm_obtain_state(term.get());
        vterm_screen_enable_altscreen(screen, 1);
        vterm_screen_enable_reflow(screen, 1);
        vterm_state_set_bold_highbright(state, 1);
        static const VTermScreenCallbacks callbacks = [] {
            VTermScreenCallbacks hooks{};
            hooks.movecursor = [](VTermPos, VTermPos, int visible, void *user) {
                static_cast<Model *>(user)->cursorVisible = visible != 0;
                return 1;
            };
            hooks.settermprop = [](VTermProp prop, VTermValue *val, void *user) {
                auto &self = *static_cast<Model *>(user);
                switch (prop) {
                case VTERM_PROP_ALTSCREEN:
                    self.alternate = val->boolean != 0;
                    ++self.revision;
                    break;
                case VTERM_PROP_CURSORVISIBLE: self.cursorVisible = val->boolean != 0; break;
                case VTERM_PROP_CURSORSHAPE: self.cursorShape = val->number; break;
                case VTERM_PROP_MOUSE: self.mouseMode = val->number; break;
                default: break;
                }
                return 1;
            };
            hooks.sb_pushline4 = [](int count, const VTermScreenCell *cells, int continued, void *user) {
                auto &self = *static_cast<Model *>(user);
                if (self.historyLimit) self.history.push_back({{cells, cells + count}, continued != 0});
                if (!self.historyLimit || self.history.size() > self.historyLimit) {
                    if (self.historyLimit) self.history.pop_front();
                    ++self.historyBase;
                }
                return 1;
            };
            hooks.sb_popline = [](int count, VTermScreenCell *cells, void *user) {
                auto &self = *static_cast<Model *>(user);
                if (self.history.empty()) return 0;
                const auto &line = self.history.back().cells;
                VTermScreenCell blank{};
                blank.width = 1;
                blank.fg.type = VTERM_COLOR_DEFAULT_FG;
                blank.bg.type = VTERM_COLOR_DEFAULT_BG;
                std::fill_n(cells, count, blank);
                std::copy_n(line.begin(), std::min<int>(count, int(line.size())), cells);
                if (cells[count - 1].width == 2) cells[count - 1] = blank;
                self.history.pop_back();
                return 1;
            };
            hooks.sb_clear = [](void *user) {
                auto &self = *static_cast<Model *>(user);
                self.historyBase += self.history.size();
                self.history.clear();
                ++self.revision;
                return 1;
            };
            return hooks;
        }();
        vterm_screen_set_callbacks(screen, &callbacks, this);
        vterm_screen_callbacks_has_pushline4(screen);
        vterm_screen_reset(screen, 1);
    }

    long long first() const { return alternate ? 0 : historyBase; }
    long long screenTop() const { return first() + (alternate ? 0 : static_cast<long long>(history.size())); }

    VTermScreenCell cell(long long row, int column) const {
        VTermScreenCell result{};
        result.width = 1;
        result.fg.type = VTERM_COLOR_DEFAULT_FG;
        result.bg.type = VTERM_COLOR_DEFAULT_BG;
        if (row < screenTop()) {
            const auto &cells = history[std::size_t(row - first())].cells;
            if (column < int(cells.size())) result = cells[std::size_t(column)];
        } else {
            vterm_screen_get_cell(screen, {int(row - screenTop()), column}, &result);
        }
        return result;
    }

    bool continued(long long row) const {
        if (row <= first() || row >= screenTop() + grid.rows) return false;
        if (row < screenTop()) return history[std::size_t(row - first())].continued;
        return vterm_state_get_lineinfo(state, int(row - screenTop()))->continuation != 0;
    }

    std::string text(std::pair<long long, int> start, std::pair<long long, int> end) const {
        std::string result;
        for (auto row = start.first; row <= end.first; ++row) {
            const int firstColumn = row == start.first ? start.second : 0;
            const bool inHistory = row < screenTop();
            const int fullWidth = inHistory ? int(history[std::size_t(row - first())].cells.size()) : grid.columns;
            const int lastColumn = row == end.first ? std::min(end.second, fullWidth - 1) : fullWidth - 1;
            if (row != start.first && !continued(row)) result.push_back('\n');
            const auto lineStart = result.size();
            for (int column = firstColumn; column <= lastColumn; ++column)
                appendCell(result, cell(row, column));
            if (row == end.first || !continued(row + 1)) {
                while (result.size() > lineStart && result.back() == ' ') result.pop_back();
            }
        }
        while (!result.empty() && result.back() == '\n') result.pop_back();
        return result;
    }

    TerminalSize grid;
    unsigned historyLimit;
    std::unique_ptr<VTerm, VTermDeleter> term;
    VTermScreen *screen = nullptr;
    VTermState *state = nullptr;
    std::deque<Line> history;
    std::string input;
    long long historyBase = 0;
    unsigned revision = 0;
    bool alternate = false, cursorVisible = true;
    int cursorShape = VTERM_PROP_CURSORSHAPE_BLOCK, mouseMode = VTERM_PROP_MOUSE_NONE;
};

Terminal::Terminal(TerminalSize size, unsigned limit)
    : model_(std::make_unique<Model>(size, std::min(limit, 10000u))) {}
Terminal::~Terminal() = default;

void Terminal::reset() {
    auto fresh = std::make_unique<Model>(size(), model_->historyLimit);
    fresh->revision = model_->revision + 1;
    model_ = std::move(fresh);
}

bool Terminal::resize(TerminalSize size) {
    size = bounded(size);
    if (size.columns == model_->grid.columns && size.rows == model_->grid.rows) return false;
    model_->grid = size;
    vterm_set_size(model_->term.get(), size.rows, size.columns);
    ++model_->revision;
    return true;
}

TerminalSize Terminal::size() const { return model_->grid; }

void Terminal::feed(std::string_view bytes) {
    if (bytes.empty()) return;
    vterm_input_write(model_->term.get(), bytes.data(), bytes.size());
    vterm_screen_flush_damage(model_->screen);
}

std::string Terminal::takeInput() { return std::exchange(model_->input, {}); }

std::string Terminal::text() const {
    return model_->text({model_->first(), 0}, {model_->screenTop() + size().rows - 1, 299});
}

void Terminal::paste(std::string_view utf8) {
    // Clipboard text can contain terminal control bytes, including the bracketed paste delimiter.
    constexpr std::size_t framing = 12;
    const auto room = inputLimit - model_->input.size();
    if (utf8.empty() || room <= framing) return;
    utf8 = utf8.substr(0, std::min(utf8.size(), room - framing));
    vterm_keyboard_start_paste(model_->term.get());
    for (std::size_t offset = 0; offset < utf8.size();) {
        unsigned cp = 0;
        const int used = ImTextCharFromUtf8(&cp, utf8.data() + offset, utf8.data() + utf8.size());
        if (used <= 0) break;
        offset += used;
        if (cp == '\r' && offset < utf8.size() && utf8[offset] == '\n') ++offset;
        if (cp == '\n') cp = '\r';
        if (cp == '\r' || cp == '\t' || (cp >= 32 && (cp < 127 || cp >= 160)))
            vterm_keyboard_unichar(model_->term.get(), cp, VTERM_MOD_NONE);
    }
    vterm_keyboard_end_paste(model_->term.get());
}

bool terminalView(const char *id, Terminal &terminal, TerminalViewState &view, ImVec2 size,
                  const TerminalOptions &options) {
    auto &model = *terminal.model_;
    auto *font = monoFont();
    ImGui::PushFont(font, 0);
    const auto &p = palette();
    const auto available = ImGui::GetContentRegionAvail();
    const float pad = dp(8), rail = dp(8), border = dp(1), fontSize = ImGui::GetFontSize();
    const float cw = std::ceil(ImGui::CalcTextSize("M").x), ch = std::ceil(fontSize + dp(3));
    size.x = std::max(dp(32), size.x > 0.f ? size.x : available.x);
    size.y = std::max(ch * 2 + pad * 2, size.y > 0.f ? size.y : available.y);
    const auto pos = ImGui::GetCursorScreenPos();
    const ImVec2 origin{pos.x + pad, pos.y + pad};
    const ImVec2 bottom{pos.x + size.x, pos.y + size.y};
    const bool resized = terminal.resize({int((size.x - pad * 2 - rail) / cw), int((size.y - pad * 2) / ch)});
    if (view.revision != model.revision) {
        view.anchorRow = view.selectionRow = -1;
        view.selecting = false;
        view.follow = true;
        view.revision = model.revision;
    }

    auto &g = *ImGui::GetCurrentContext();
    auto &io = g.IO;
    auto *window = ImGui::GetCurrentWindow();
    const auto item = window->GetID(id);
    const ImRect bounds(pos, bottom);
    ImGui::ItemSize(bounds);
    if (!ImGui::ItemAdd(bounds, item, nullptr, options.interactive ? 0 : ImGuiItemFlags_NoNav)) {
        ImGui::PopFont();
        return resized;
    }
    const bool hovered = ImGui::ItemHoverable(bounds, item, g.LastItemData.ItemFlags);
    const bool clicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    if (g.ActiveId == item && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !hovered)
        ImGui::ClearActiveID();
    if (options.interactive && (clicked || options.focus || g.NavActivateId == item)) {
        ImGui::SetActiveID(item, window);
        ImGui::FocusWindow(window);
        ImGui::SetFocusID(item, window);
    }
    if (!options.interactive && g.ActiveId == item) ImGui::ClearActiveID();
    const bool focused = options.interactive && g.ActiveId == item && !io.AppFocusLost;
    if (focused != view.focused) {
        if (focused) vterm_state_focus_in(model.state);
        else vterm_state_focus_out(model.state);
        view.focused = focused;
    }

    const auto first = model.first(), lastTop = model.screenTop();
    if (view.follow) view.topRow = lastTop;
    view.topRow = std::clamp(view.topRow, first, lastTop);
    const auto scroll = [&](long long to) {
        view.topRow = std::clamp(to, first, lastTop);
        view.follow = view.topRow == lastTop;
    };
    const float railX = bottom.x - pad - rail;
    const bool railHovered = hovered && io.MousePos.x >= railX;
    const bool mouseMode = options.interactive && model.mouseMode != VTERM_PROP_MOUSE_NONE && view.follow;
    if (hovered && !railHovered && (!mouseMode || io.KeyShift)) ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
    const auto mouseRow = [&] {
        return std::clamp(int(std::floor((io.MousePos.y - origin.y) / ch)), 0, model.grid.rows - 1);
    };
    const auto mouseColumn = [&] {
        return std::clamp(int(std::floor((io.MousePos.x - origin.x) / cw)), 0, model.grid.columns - 1);
    };
    const auto modifiers = VTermModifier((io.KeyCtrl ? VTERM_MOD_CTRL : 0) |
        (io.KeyAlt ? VTERM_MOD_ALT : 0) | (io.KeyShift ? VTERM_MOD_SHIFT : 0));
    if (hovered && io.MouseWheel != 0) {
        ImGui::SetKeyOwner(ImGuiKey_MouseWheelY, item);
        if (mouseMode && !io.KeyShift && !railHovered) {
            vterm_mouse_move(model.term.get(), mouseRow(), mouseColumn(), modifiers);
            vterm_mouse_button(model.term.get(), io.MouseWheel > 0 ? 4 : 5, 1, modifiers);
        } else {
            scroll(view.topRow - std::lround(io.MouseWheel * 3));
        }
    }
    if (clicked && railHovered && first != lastTop) {
        const float ratio = std::clamp((io.MousePos.y - origin.y) / (ch * model.grid.rows), 0.f, 1.f);
        scroll(first + std::llround(ratio * float(lastTop - first)));
    }
    if (clicked && !railHovered) {
        view.reportingMouse = mouseMode && !io.KeyShift;
        if (view.reportingMouse) {
            vterm_mouse_move(model.term.get(), mouseRow(), mouseColumn(), modifiers);
            vterm_mouse_button(model.term.get(), 1, 1, modifiers);
        } else {
            view.anchorRow = view.selectionRow = view.topRow + mouseRow();
            view.anchorColumn = view.selectionColumn = mouseColumn();
            view.selecting = true;
        }
    }
    if (view.reportingMouse || (mouseMode && hovered && !railHovered && !io.KeyShift && !view.selecting))
        vterm_mouse_move(model.term.get(), mouseRow(), mouseColumn(), modifiers);
    if (view.reportingMouse && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        vterm_mouse_button(model.term.get(), 1, 0, modifiers);
        view.reportingMouse = false;
    }
    if (view.selecting) {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) view.selecting = false;
        if (view.selecting || ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            view.selectionRow = view.topRow + mouseRow();
            view.selectionColumn = mouseColumn();
        }
    }
    // Scan only visible rows; keep the selection anchored as scrollback grows.
    auto a = std::pair{view.anchorRow, view.anchorColumn};
    auto b = std::pair{view.selectionRow, view.selectionColumn};
    if (b < a) std::swap(a, b);
    const bool selection = a.first >= first && b.first < lastTop + model.grid.rows && a != b;
    const auto copy = [&] {
        ImGui::SetClipboardText((selection ? model.text(a, b) : terminal.text()).c_str());
    };
    const auto paste = [&] {
        if (const char *content = ImGui::GetClipboardText()) terminal.paste(content);
        scroll(lastTop);
    };
    if (!mouseMode || io.KeyShift || railHovered) {
        if (ImGui::BeginPopupContextItem("##terminal-menu")) {
            if (ImGui::MenuItem(trLabel("Copy").c_str())) copy();
            if (ImGui::MenuItem(trLabel("Paste").c_str(), nullptr, false, options.interactive)) paste();
            ImGui::EndPopup();
        }
    }

    VTermPos cursor{};
    vterm_state_get_cursorpos(model.state, &cursor);
    if (focused) {
        ImGui::SetActiveIdUsingAllKeyboardKeys();
        ImGui::SetKeyOwner(ImGuiKey_Escape, item);
        ImGui::SetNextFrameWantCaptureKeyboard(true);
        auto &ime = g.PlatformImeData;
        ime.WantTextInput = ime.WantVisible = true;
        ime.InputPos = ImVec2(origin.x + cursor.col * cw, origin.y + cursor.row * ch);
        ime.InputLineHeight = ch;
        ime.ViewportId = window->Viewport->ID;

        const bool clipboardModifier = io.KeySuper || (io.KeyCtrl && io.KeyShift);
        const auto pressed = [&](ImGuiKey key) { return ImGui::IsKeyPressed(key, ImGuiInputFlags_Repeat, item); };
        if (clipboardModifier && pressed(ImGuiKey_C)) copy();
        if ((clipboardModifier && pressed(ImGuiKey_V)) || (io.KeyShift && pressed(ImGuiKey_Insert))) paste();
        if (io.KeyShift && pressed(ImGuiKey_PageUp)) scroll(view.topRow - model.grid.rows + 1);
        if (io.KeyShift && pressed(ImGuiKey_PageDown)) scroll(view.topRow + model.grid.rows - 1);
        constexpr std::pair<ImGuiKey, VTermKey> keys[] = {
            {ImGuiKey_Enter, VTERM_KEY_ENTER}, {ImGuiKey_KeypadEnter, VTERM_KEY_KP_ENTER},
            {ImGuiKey_Tab, VTERM_KEY_TAB}, {ImGuiKey_Backspace, VTERM_KEY_BACKSPACE},
            {ImGuiKey_Escape, VTERM_KEY_ESCAPE}, {ImGuiKey_UpArrow, VTERM_KEY_UP},
            {ImGuiKey_DownArrow, VTERM_KEY_DOWN}, {ImGuiKey_LeftArrow, VTERM_KEY_LEFT},
            {ImGuiKey_RightArrow, VTERM_KEY_RIGHT}, {ImGuiKey_Insert, VTERM_KEY_INS},
            {ImGuiKey_Delete, VTERM_KEY_DEL}, {ImGuiKey_Home, VTERM_KEY_HOME},
            {ImGuiKey_End, VTERM_KEY_END}, {ImGuiKey_PageUp, VTERM_KEY_PAGEUP},
            {ImGuiKey_PageDown, VTERM_KEY_PAGEDOWN},
        };
        for (const auto &[imguiKey, termKey] : keys) {
            if (io.KeyShift && (imguiKey == ImGuiKey_PageUp || imguiKey == ImGuiKey_PageDown || imguiKey == ImGuiKey_Insert))
                continue;
            if (pressed(imguiKey)) {
                vterm_keyboard_key(model.term.get(), termKey, modifiers);
                scroll(lastTop);
            }
        }
        for (int k = 0; k < 12; ++k) {
            if (pressed(ImGuiKey(ImGuiKey_F1 + k))) {
                vterm_keyboard_key(model.term.get(), VTermKey(VTERM_KEY_FUNCTION(k + 1)), modifiers);
                scroll(lastTop);
            }
        }
        const bool textInput = !io.KeySuper && (!io.KeyCtrl || io.KeyAlt);
        for (unsigned cp : io.InputQueueCharacters) {
            if (textInput && cp >= 32 && cp != 127) {
                vterm_keyboard_unichar(model.term.get(), cp, io.KeyCtrl ? VTERM_MOD_NONE : modifiers);
                scroll(lastTop);
            }
        }
        io.InputQueueCharacters.resize(0);
        if (io.KeyCtrl && !io.KeyAlt && !clipboardModifier) {
            for (unsigned k = 0; k < 26; ++k) {
                if (pressed(ImGuiKey(ImGuiKey_A + k))) {
                    vterm_keyboard_unichar(model.term.get(), 'a' + k, modifiers);
                    scroll(lastTop);
                }
            }
            if (pressed(ImGuiKey_LeftBracket)) vterm_keyboard_unichar(model.term.get(), '[', modifiers);
            if (pressed(ImGuiKey_Backslash)) vterm_keyboard_unichar(model.term.get(), '\\', modifiers);
            if (pressed(ImGuiKey_RightBracket)) vterm_keyboard_unichar(model.term.get(), ']', modifiers);
        }
    }

    auto *draw = ImGui::GetWindowDrawList();
    const float corner = ImGui::GetStyle().FrameRounding;
    draw->AddRectFilled(pos, bottom, p.bg0, corner);
    draw->AddRect(pos, bottom, focused ? p.accent : p.line, corner, 0, border);
    draw->PushClipRect(origin, {railX, std::min(bottom.y - pad, origin.y + model.grid.rows * ch)}, true);
    const Colors colors{p.bg0, p.error, p.success, p.warning, p.info, p.codeType, p.codeFunction, p.textDim,
                        p.textFaint, p.error, p.codeComment, p.codeNumber, p.codeKeyword, p.codeType,
                        p.codeFunction, p.text};
    std::string glyph;
    for (int y = 0; y < model.grid.rows; ++y) {
        const auto row = view.topRow + y;
        for (int x = 0; x < model.grid.columns; ++x) {
            const auto cell = model.cell(row, x);
            if (rightHalf(cell)) continue;
            const ImVec2 start{origin.x + x * cw, origin.y + y * ch};
            const ImVec2 end{start.x + std::max(1, int(cell.width)) * cw, start.y + ch};
            auto fg = displayColor(model.screen, cell.fg, colors), bg = displayColor(model.screen, cell.bg, colors);
            if (cell.attrs.reverse) std::swap(fg, bg);
            if (bg != p.bg0) draw->AddRectFilled(start, end, bg);
            const auto location = std::pair{row, x};
            if (selection && a <= std::pair{row, x + std::max(1, int(cell.width)) - 1} && location <= b)
                draw->AddRectFilled(start, end, p.selection);
            if (cell.attrs.conceal) continue;
            if (!cell.chars[0] && !cell.attrs.underline && !cell.attrs.strike) continue;
            fg = contrastInk(fg, bg, p);
            glyph.clear();
            if (cell.chars[0]) {
                appendCell(glyph, cell);
                draw->AddText(font, fontSize, {start.x, start.y + (ch - fontSize) * .5f}, fg, glyph.data(), glyph.data() + glyph.size());
            }
            if (cell.attrs.underline)
                draw->AddLine({start.x, end.y - border * 2}, {end.x, end.y - border * 2}, fg, border);
            if (cell.attrs.strike)
                draw->AddLine({start.x, start.y + ch * .5f}, {end.x, start.y + ch * .5f}, fg, border);
        }
    }
    if (view.follow && model.cursorVisible && cursor.row < model.grid.rows && cursor.col < model.grid.columns) {
        const ImVec2 start{origin.x + cursor.col * cw, origin.y + cursor.row * ch};
        const ImVec2 end{start.x + cw, start.y + ch};
        const auto color = focused ? p.accent : p.textDim;
        if (model.cursorShape == VTERM_PROP_CURSORSHAPE_BAR_LEFT)
            draw->AddRectFilled(start, {start.x + border * 2, end.y}, color);
        else if (model.cursorShape == VTERM_PROP_CURSORSHAPE_UNDERLINE)
            draw->AddRectFilled({start.x, end.y - border * 2}, end, color);
        else
            draw->AddRect(start, end, color, 0, 0, border);
    }
    draw->PopClipRect();
    if (lastTop > first) {
        const float height = ch * model.grid.rows;
        const float fraction = float(model.grid.rows) / float(lastTop - first + model.grid.rows);
        const float thumb = std::max(dp(12), height * fraction);
        const float top = origin.y + (height - thumb) * float(view.topRow - first) / float(lastTop - first);
        draw->AddRectFilled({railX + border, origin.y}, {railX + rail, origin.y + height}, p.bg2, rail * .5f);
        draw->AddRectFilled({railX + border, top}, {railX + rail, top + thumb},
                            railHovered ? p.textFaint : p.bg5, rail * .5f);
    }
    ImGui::PopFont();
    return resized;
}

} // namespace cyui
