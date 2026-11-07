---
id: widgets
title: Widget API reference
description: Widget API reference for CyUI integrators.
section: API
order: 20
revision: 1
---

All functions are in `namespace cyui`. The headers carry the full comments.

**Units.** Widget `size` and `width` parameters are screen pixels: wrap design
values in `dp()`, as in `cyui::button("OK", cyui::dp(80, 0))`. 0 means
automatic, and a negative width fills the row minus that much. These are
design units, scaled inside:
- `beginCard` height
- `beginProperties` label width
- `beginSidebar` and `beginModal` widths
- `space` height, `divider` spacing, `spinner` radius
- `KnobStyle::diameter`

`beginTopBar`'s height is in pixels.

**Disabled and edited.** Inside `ScopedDisabled`, CyUI's hand-drawn widgets
fade and stop reacting, like stock ImGui. Every CyUI control that changes a
value marks it edited, so `ImGui::IsItemEdited()` is true after it on that
frame. Clicks and drags also give `ImGui::IsItemDeactivatedAfterEdit()` on
release: one undo step per gesture. Wheel nudges and typed values are one-shot
edits that only set `IsItemEdited()`. `markEdited(id)` does the same for your
own controls.

### Theme (`Theme.h`) {#theme-themeh}

- **Setup:** `apply(Config)`, `applyDpi(s)`, `dpi()` (the current scale),
  `loadFonts(FontConfig)`.
- **Palette:** `palette()`, `setPalette(p)`, `setTheme("key")`,
  `builtInThemes()`, `currentThemeKey()`. `paletteFields()` lists each role
  with its key, label, group and member pointer, for editors and theme files.
- **Density and metrics:** `setDensity(d)`, `density()`, `metrics()`. The
  metrics give the font size, paddings, roundings, `cardPadding` and
  `toolbarHeight`.
- **Fonts:** `uiFont()`, `boldFont()`, `monoFont()`, `iconsAvailable()`. The
  fonts are null until `loadFonts` filled the context's atlas.
  `ScopedFont(nullptr, factor)` then keeps the current font.
- **Colour helpers:** `rgb(0xRRGGBB, a)`, `alpha`, `mix`, `brighter`,
  `darker`, `vec`, `categorical(i)`. `categorical` gives 12 hues derived from
  the palette, toned for light or dark grounds.
- **Sizes:** `dp(v)` / `dp(x, y)`.
- **Theme files:** `savePalette(path, p)`, `loadPalette(path, p)`,
  `paletteToText`, `paletteFromText`. The `.cytheme` format is
  `key = #RRGGBB[AA]`; CyDAW's JSON `.cytheme` files also load.

### Icons (`Icons.h`) {#icons-iconsh}

`cyui::icon::play, pause, stop, record, add, remove, trash, settings, search,
folder, save, undo, redo, edit, close, check, chevronDown, more, refresh,
zoomIn, lock, filter, loop, library, star, copy, paste, info, warning, home,
globe, people, mail, calendar, clock, cloud, upload, download, code, photo,
document, ...` (104 icons). They are UTF-8 strings of Segoe MDL2 Assets, so
they work in any label: `std::string(icon::save) + "  Save"`. They draw only
on Windows. Without the icon font, the CyUI widgets fall back:
- `iconButton` shows the first letter of its tooltip when the current font
  lacks the glyph (a host's own merged icon font still works).
- `iconTextButton` shows the label alone.
- `sidebarItem` and `emptyState` leave the icon out.
- `helpMarker` shows `(?)`.

Guard your own icon labels with `iconsAvailable()`.

### Text (`CoreWidgets.h`) {#text-corewidgetsh}

- `heading(t, level)`, `sectionTitle(t)`, `overline(T)`
- `textDim(fmt, ...)`, `textFaint(fmt, ...)`, `textColoured(c, fmt, ...)`,
  `textMono(fmt, ...)`
- `kbd("Ctrl+S")`, `helpMarker(t)`, `tooltip(t)`
- `clippedText(list, pos, w, c, t)`, `centredText(list, min, max, c, t)`

### Buttons {#buttons}

- `toneColour(Tone)`: the palette colour of a tone (neutral is `textDim`).
- `button(label, Variant, size)`. The variants are `secondary`, `primary`,
  `ghost`, `outline`, `danger` and `success`.
- Shortcuts: `primaryButton`, `dangerButton`, `ghostButton`.
- `iconTextButton(icon, label, variant)`
- `iconButton(icon, tooltip, active, activeColour, size)`
- `toggleButton(label, &b, onColour, size, tooltip)`: compact pills such as
  M, S and R.
- `toggleSwitch(label, &b)`
- `segmented(id, labels, n, &current, width)`. A width of `-1` fills the row.
- `linkText(label)`

### Values {#values}

- `knob(id, &v, min, max, default, KnobStyle{diameter, colour, label, format,
  unit})`. Set `KnobStyle.flags` to `knobBipolar`, `knobLogarithmic`,
  `knobInteger` or `knobNoLabel`. `.steps` and `.modulation` are also
  available.
- `fader(id, &dB, minDb, maxDb, size, colour)`
- `meter(id, size, peakL, peakR, rmsL, rmsR, vertical)`
- `dragValue(id, &v, speed, min, max, fmt, width)`. `speed` is per pixel
  dragged. Once the field has the focus (clicked or navigated to), a wheel
  notch moves 10 × `speed`; before that, the wheel scrolls the panel.
  `min >= max` is unbounded.
- `sliderBar(id, &v, min, max, fmt, width, colour)`. A click or a drag sets the
  value. A wheel notch moves 1 % of the range.
- `valueEntry(...)`: the typed number field these controls open.
  `valueEntryOpen()` says whether one is open (to hold back shortcuts while
  the user types).
- `itemWheel()`: the wheel for your own control, claimed in time so the panel
  under it does not scroll. Also `takeWheel()`, `wheelTaken()` and
  `valueSpeed()`.

The value controls share these interactions:
- drag to change; Ctrl is fine (× 0.15), Shift is fast (× 4)
- the mouse wheel nudges the value and does not scroll the panel under it
  (a `dragValue` only once it has the focus)
- a double click types the value (`dragValue`, `sliderBar`) or resets it (a
  knob)
- a click on a knob's value types it

### Fields {#fields}

- `searchField(id, buf, size, hint, width)`
- `textField(id, buf, size, hint, width)`
- `combo(id, &i, items, n, width)`

### Quick filtering (`Filter.h`, included in Core) {#quick-filtering-filterh-included-in-core}

Provide a quick filter in menus and panels where users need to find actions or
properties. Keep a `TextFilter` in each view's state. `filterBox(id, state)` draws
the query field; `panelHeader(title, subtitle, &state)` includes it in the heading.
Use `state.matches(englishLabel, keywordsAndSectionPath)` before drawing a row.
Search uses all words, ignoring ASCII/Latin case and accents, and checks both the
English key and the current translation. Hidden ImGui IDs are excluded.

`beginFilteredSection(label, state, hasMatchingChildren, defaultOpen)` reveals a
matching section without changing its saved collapse preference; pair with
`endSection()` when true. `filterMenu(id, state, entries)` draws a searchable menu
inside an already open ImGui menu/popup. Its `FilterMenuItem` tree retains action
IDs, disabled/checked states and shortcuts. Searching lists matching descendants
with breadcrumbs. The host handles the returned action ID.

`InspectorOptions{&state, sectionTitle}` enables filtering in `inspector()`;
`propertyMatches(state, property, sectionTitle)` helps choose visible sections.
Call Inspector for a section's full property model so hidden active edits can be
cancelled; when omitting an entire panel, cancel its host previews explicitly.
Read [FILTERING.md](doc:filtering) for complete C++ and Python examples and limits.
These helpers do not intercept raw ImGui calls or patch Dear ImGui.

### Containers {#containers}

- `beginCard(id, title, height, tone)` / `endCard()`. **Always** call
  `endCard`, whatever `beginCard` returned.
- `beginSection(label, defaultOpen)` / `endSection()`. Call `endSection`
  **only when** `beginSection` returned true.
- `beginProperties(id, labelWidth)` / `property(label, help)` /
  `endProperties()`. Call `endProperties` only when `beginProperties`
  returned true. `property()` sets up full width for the next widget.
- `toolbarSeparator()`, `divider(spacing)`, `space(h)`

### Status {#status}

- `badge(t, Tone)`, `statusDot(c, tip)`
- `progressBar(f, size, overlay, Tone)`. A fraction `f < 0` gives an
  indeterminate bar.
- `spinner(id, r, c)`
- `emptyState(title, msg, icon)`
- `callout(Tone, text)`
- `colourChip(id, c, size)`
- `notify(text, Tone, seconds)` shows a toast in the current ImGui context.
  With no context current, it goes to the one context CyUI knows. When there
  are several or none, it asserts in debug builds and drops the toast.
  `notify(context, text, Tone, seconds)` posts to a given context, from any
  thread. Call `renderNotifications()` once per frame. At most six toasts
  show at a time.

### Layout (`Layout.h`) {#layout-layouth}

The application shell. Call these in the frame in this order: menu bar, top
bar, status bar, sidebar, then dock space.

- `beginTopBar(id, h)` / `endTopBar()`
- `beginStatusBar(id)` / `endStatusBar()`, with `statusText(Tone, fmt, ...)`
- `beginSidebar(id, w)` / `endSidebar()`, with `sidebarHeader(title, badge)`,
  `sidebarGroup(LABEL)` and `sidebarItem(icon, label, active, badge)`
- `dockSpace(id, flags)` (docking branch)
- `panelHeader(title, subtitle)`
- `panelHeader(title, subtitle, &filter)` to add a quick filter
- `beginToolbar(id)` / `endToolbar()`
- `beginModal(title, &open, width)` / `endModal()`. Call `endModal` only
  when `beginModal` returned true. `modalButtons(confirm, cancel, danger)`
  returns 1 for confirm and 2 for cancel. Enter confirms and Escape cancels.
  Modals can nest: each `modalButtons` closes its own modal.
- `alignRight(width)`, `buttonWidth(label)`

All `begin*Bar` / `beginSidebar` / `beginToolbar` must be paired with their
`end*`, whatever they returned. Popups and tooltips opened from a bar keep the
theme's style.

### Gallery (`Gallery.h`) {#gallery-galleryh}

- `showGallery(&open, selectTab)`: every widget, a theme switcher and a
  density switcher. `selectTab` brings a page to the front once:
  - 0 text, 1 buttons, 2 controls, 3 containers
  - 4–19 the optional modules
  - 20 layout (the application shell), 21 filtering (menus and panels)
- `showGallery(&open, selectTab, GalleryResources)`: the same, with host
  resources for the module pages: `texture`, `width` and `height` (Images,
  Viewport), `web` (a live Web backend) and `previewCommands`.
- `showThemeEditor(&open)`: live palette editing and `.cytheme`
  save and load.

`GalleryResources::openDocumentation(page, section)` optionally routes the
Filtering example’s Help action to the host’s local reader at
`filtering/searchable-menus`. Keep its callback owner alive during drawing.
Gallery does not launch a browser itself.

---
