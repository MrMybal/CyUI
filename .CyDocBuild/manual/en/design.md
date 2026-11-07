---
id: design
title: Themes, layout and migration
description: Themes, layout and migration for CyUI integrators.
section: Guides
order: 11
revision: 1
---

1. **Never hard-code a colour.** Take every colour from `cyui::palette()`,
   by its role:
   - `bg0` is the deepest ground: canvases, timelines, wells, the status bar.
   - `bg1` is the panel ground.
   - `bg2` is for headers, cards and popups.
   - `bg3` is for fields and buttons at rest; `bg4` when hovered; `bg5` when
     pressed or active.
   - `line` and `lineStrong` are for lines.
   - `text`, `textDim` and `textFaint` are the three text levels.
   - `accent`, `accentHover`, `accentDim`, `accentText` and `selection`: there
     is **one** accent, used only for what is active, selected or the main
     action.
   - `success`, `warning`, `error` and `info` carry meaning.

   For a variant, use `cyui::alpha / mix / brighter / darker`. For series of
   things (tracks, layers, tags), use `cyui::categorical(i)`.
2. **Never hard-code a size in pixels.** Wrap it: `cyui::dp(12.0f)`,
   `cyui::dp(8, 4)`. Prefer sizes derived from the style:
   `ImGui::GetFrameHeight()`, `ImGui::GetFontSize()`, `ImGui::GetStyle()...`.
3. **Text hierarchy.**
   - `cyui::heading(t, 1|2|3)` for titles.
   - `cyui::sectionTitle` for groups in a panel.
   - `cyui::overline("UPPERCASE")` for compact uppercase labels.
   - `cyui::textDim` for labels and secondary info.
   - `cyui::textFaint` for hints.

   Do not make titles with `ImGui::Text` plus a colour push.
4. **One primary button per view.** Use `cyui::primaryButton` for the main
   action, `cyui::button` (secondary) for the rest, `cyui::ghostButton` or
   `cyui::iconButton` in toolbars, and `cyui::dangerButton` for destructive
   actions.
5. **Toolbars are icon buttons.** Use `cyui::iconButton(icon::x, "Tooltip
   (Shortcut)", active)`. Separate groups with `cyui::toolbarSeparator()`.
   Every icon button **must** have a tooltip that names the action and its
   shortcut.
6. **Settings and inspectors are property tables**
   (`beginProperties` / `property` / `endProperties`), inside collapsible
   sections (`beginSection`). Do not use free-floating
   `ImGui::Text("Label:"); ImGui::SameLine(); ImGui::SliderFloat(...)` lines.
7. **Group related content in cards** (`beginCard` / `endCard`). Use toned
   cards (`Tone::accent / warning / error`) only to draw attention.
8. **Empty panels say why.** Use `cyui::emptyState("No layers yet", "Drop an
   image or press Ctrl+N", icon::photo)`. Never leave a blank panel.
9. **Feedback is visible.**
   - `cyui::notify("Saved", Tone::success)` for events.
   - `cyui::callout(Tone::warning, ...)` for standing conditions.
   - `progressBar` / `spinner` for work in progress.
   - `badge` and `statusDot` for state.
10. **Tooltips are delayed and wrapped.** Use `cyui::tooltip(text)` after an
    item. Use `cyui::helpMarker(text)` to explain an option.
11. Keep stock ImGui wherever it already reads well: `Checkbox`, `InputText`,
    `ColorEdit`, tables, trees, menus, tab bars, `DragFloat2`. The theme
    styles them. Do not wrap or reinvent them.
12. Prefer the RAII helpers `ScopedColor`, `ScopedVar`, `ScopedFont`,
    `ScopedID` and `ScopedDisabled` over manual Push/Pop pairs.

---


## Migrate an ImGui interface {#migrate-an-imgui-interface}


Work panel by panel. Change the look, not the behaviour. Keep IDs, labels
and logic as they are.

| Stock / old code | CyUI |
|---|---|
| `ImGui::StyleColorsDark(); style.Colors[...] = ...` (theme function) | delete it; `cyui::apply(config); cyui::loadFonts();` |
| `IM_COL32(40,42,50,255)` or `ImVec4(...)` literals in drawing code | `cyui::palette().bg3` (pick by role) |
| `10.0f` pixel literals in custom drawing | `cyui::dp(10.0f)` |
| `ImGui::Button("OK")` main action of a dialog/panel | `cyui::primaryButton("OK")` |
| `ImGui::Button("Delete")` | `cyui::dangerButton("Delete")` |
| `ImGui::Button("X")` / `ImGui::SmallButton` in toolbars | `cyui::iconButton(cyui::icon::close, "Close")` |
| `ImGui::Button(icon)` + `if (selected) PushStyleColor(...)` tool buttons | `cyui::iconButton(icon, tip, selected)` |
| `ImGui::Checkbox` for an on/off **setting** | `cyui::toggleSwitch("Label", &v)` (keep `Checkbox` in lists/tables) |
| row of `RadioButton`s / buttons for a mode | `cyui::segmented("##mode", labels, n, &mode)` |
| `ImGui::Text("Section"); ImGui::Separator();` | `cyui::sectionTitle("Section")` or `cyui::beginSection("Section")` |
| `ImGui::TextColored(gray, ...)` | `cyui::textDim(...)` / `cyui::textFaint(...)` |
| `ImGui::Text("Name:"); SameLine(); InputText(...)` stacks | `beginProperties` + `property("Name")` + the widget |
| `ImGui::SliderFloat` in an inspector | `cyui::sliderBar(id, &v, min, max, "%.2f", -1)` (or keep SliderFloat) |
| `ImGui::InputText` used as a filter | `cyui::searchField("##filter", buf, size, "Search")` |
| `ImGui::BeginChild(..., true)` used as a box | `cyui::beginCard(id, "Title")` ... `cyui::endCard()` |
| `ImGui::ProgressBar` | `cyui::progressBar(f, size, overlay, tone)` |
| `ImGui::OpenPopup` + `BeginPopupModal` confirm dialog | `cyui::beginModal` + `cyui::modalButtons` |
| status messages printed in a panel | `cyui::notify(...)` |
| custom main-window bars with `SetNextWindowPos` hacks | `cyui::beginTopBar` / `beginStatusBar` / `beginSidebar` |
| audio knobs / faders / meters drawn by hand | `cyui::knob` / `cyui::fader` / `cyui::meter` |

Custom-drawn views (canvases, node editors, timelines, curves) keep their
drawing code. Swap their colour constants for palette roles:
- grounds: `bg0`
- grid: `grid` / `gridStrong`
- selection: `selection` plus an `accent` outline
- text: `text` / `textDim`
- handles: `bg5`, or `accent` when active

Check your work: open `cyui::showGallery()` from a debug menu, switch themes
and densities, and look at the app in **all five themes**. Nothing may stay
dark-on-dark or light-on-light.

---

## Compare themes {#themes}

```gallery
{"id": "themes", "controls": "minimal", "items": [{"type": "image", "src": "assets/overview.webp", "alt": "Graphite and orange", "title": "Graphite and orange", "description": "The same gallery in another theme."}, {"type": "image", "src": "assets/theme-light.webp", "alt": "Light grey", "title": "Light grey", "description": "The same gallery in another theme."}, {"type": "image", "src": "assets/theme-amber.webp", "alt": "Amber night", "title": "Amber night", "description": "The same gallery in another theme."}, {"type": "image", "src": "assets/theme-violet.webp", "alt": "Violet", "title": "Violet", "description": "The same gallery in another theme."}, {"type": "image", "src": "assets/theme-navy.webp", "alt": "Navy", "title": "Navy", "description": "The same gallery in another theme."}]}
```
