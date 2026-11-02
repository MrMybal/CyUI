#include "cyui/Gallery.h"

#include "cyui/Icons.h"
#include "cyui/Layout.h"
#include "cyui/Theme.h"
#include "cyui/CoreWidgets.h"

#if defined(CYUI_HAS_AUDIO)
#include "cyui/modules/Audio.h"
#endif
#if defined(CYUI_HAS_CODEEDITOR)
#include <TextEditor.h>
#endif

#include <imgui.h>
#include "cyui/CyUI.h"
#include <cstdint>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace cyui {
namespace {

void themeBar()
{
    const auto& themes = builtInThemes();
    static const char* names[16] {};
    auto current = 0;
    const auto count = static_cast<int>(std::min<std::size_t>(themes.size(), 16));
    for (auto i = 0; i < count; ++i) {
        names[i] = themes[static_cast<std::size_t>(i)].name.c_str();
        if (themes[static_cast<std::size_t>(i)].key == currentThemeKey())
            current = i;
    }
    ImGui::AlignTextToFramePadding();
    textDim("Theme");
    ImGui::SameLine();
    if (combo("##theme", &current, names, count, dp(200.0f)))
        setTheme(themes[static_cast<std::size_t>(current)].key.c_str());
    ImGui::SameLine();
    static const char* densities[] { "Compact", "Comfortable" };
    auto dense = density() == Density::compact ? 0 : 1;
    if (segmented("##density", densities, 2, &dense))
        setDensity(dense == 0 ? Density::compact : Density::comfortable);
}

void palettePreview()
{
    const auto& p = palette();
    for (const auto& field : paletteFields()) {
        ImGui::PushID(field.key);
        colourChip("##chip", p.*field.member, dp(22.0f, 22.0f));
        tooltip(field.label);
        ImGui::PopID();
        ImGui::SameLine(0.0f, dp(3.0f));
    }
    ImGui::NewLine();
}

void textPage()
{
    heading("Heading 1", 1);
    heading("Heading 2", 2);
    heading("Heading 3", 3);
    sectionTitle("Section title");
    overline("OVERLINE LABEL");
    ImGui::TextUnformatted("Primary text");
    textDim("Secondary text");
    textFaint("Faint text, hints");
    textColoured(palette().info, "Coloured text (a palette role)");
    textMono("C:/Projects/mono/path.txt");
    ImGui::TextUnformatted("Shortcut");
    ImGui::SameLine();
    kbd("Ctrl+S");
    ImGui::SameLine();
    textDim("Help");
    helpMarker("A help marker: its tooltip explains the option before it.");
    ImGui::SameLine(0.0f, dp(16.0f));
    if (linkText("A link"))
        notify("Link clicked", Tone::info);
    space();
    badge("NEUTRAL");
    ImGui::SameLine();
    badge("ACCENT", Tone::accent);
    ImGui::SameLine();
    badge("OK", Tone::success);
    ImGui::SameLine();
    badge("BETA", Tone::warning);
    ImGui::SameLine();
    badge("ERROR", Tone::error);
    ImGui::SameLine();
    badge("INFO", Tone::info);
    statusDot(palette().success, "Online");
    ImGui::SameLine();
    ImGui::TextUnformatted("Online");
    ImGui::SameLine(0.0f, dp(14.0f));
    statusDot(palette().error, "Offline");
    ImGui::SameLine();
    ImGui::TextUnformatted("Offline");
    space();
    callout(Tone::info, "A callout gives context to a whole panel: what it does, or why it is empty.");
    space(4.0f);
    callout(Tone::warning, "Unsaved changes will be lost when the project closes.");
}

void buttonsPage()
{
    button("Secondary");
    ImGui::SameLine();
    primaryButton("Primary");
    ImGui::SameLine();
    ghostButton("Ghost");
    ImGui::SameLine();
    button("Outline", Variant::outline);
    ImGui::SameLine();
    dangerButton("Delete");
    ImGui::SameLine();
    button("Confirm", Variant::success);

    iconTextButton(icon::save, "Save");
    ImGui::SameLine();
    iconTextButton(icon::folder, "Open", Variant::ghost);
    ImGui::SameLine();
    iconTextButton(icon::add, "New project", Variant::primary);
    ImGui::SameLine();
    {
        ScopedDisabled disabled;
        button("Disabled");
    }

    space();
    sectionTitle("Toolbar");
    static auto tool = 0;
    static auto playing = false;
    static auto loop = true;
    const char* tools[] { icon::edit, icon::crop, icon::zoomIn, icon::touch };
    const char* tips[] { "Brush (B)", "Crop (C)", "Zoom (Z)", "Hand (H)" };
    for (auto i = 0; i < 4; ++i) {
        if (iconButton(tools[i], tips[i], tool == i))
            tool = i;
        ImGui::SameLine(0.0f, dp(2.0f));
    }
    toolbarSeparator();
    if (iconButton(playing ? icon::pause : icon::play, "Play / pause (Space)", playing, palette().success))
        playing = !playing;
    ImGui::SameLine(0.0f, dp(2.0f));
    iconButton(icon::stop, "Stop");
    ImGui::SameLine(0.0f, dp(2.0f));
    if (iconButton(icon::loop, "Loop (L)", loop))
        loop = !loop;

    space();
    sectionTitle("Toggles");
    static bool mute = false, solo = true, arm = false, snap = true, dark = true;
    toggleButton("M", &mute, palette().info, ImVec2(ImGui::GetFrameHeight(), 0.0f), "Mute");
    ImGui::SameLine(0.0f, dp(2.0f));
    toggleButton("S", &solo, palette().warning, ImVec2(ImGui::GetFrameHeight(), 0.0f), "Solo");
    ImGui::SameLine(0.0f, dp(2.0f));
    toggleButton("R", &arm, palette().error, ImVec2(ImGui::GetFrameHeight(), 0.0f), "Arm");
    ImGui::SameLine(0.0f, dp(10.0f));
    toggleButton("Snap", &snap);
    toggleSwitch("Dark mode", &dark);
    ImGui::SameLine(0.0f, dp(16.0f));
    static auto autosave = false;
    toggleSwitch("Autosave", &autosave);

    space();
    sectionTitle("Segmented");
    static auto view = 1;
    const char* views[] { "List", "Grid", "Details" };
    segmented("##view", views, 3, &view);
    static auto unit = 0;
    const char* units[] { "px", "%", "cm", "in" };
    segmented("##unit", units, 4, &unit, -1.0f);
}

void controlsPage()
{
    const auto t = static_cast<float>(ImGui::GetTime());
#if defined(CYUI_HAS_AUDIO)
    static float gain = 0.5f, pan = 0.0f, cutoff = 1200.0f, voices = 8.0f;
    knob("##gain", &gain, 0.0f, 1.0f, 0.5f, { 34.0f, 0, "Gain", "%.2f" });
    ImGui::SameLine();
    KnobStyle panStyle { 34.0f, palette().info, "Pan", "%.2f" };
    panStyle.flags = knobBipolar;
    knob("##pan", &pan, -1.0f, 1.0f, 0.0f, panStyle);
    ImGui::SameLine();
    KnobStyle cutoffStyle { 34.0f, palette().success, "Cutoff", "%.0f", "Hz" };
    cutoffStyle.flags = knobLogarithmic;
    cutoffStyle.modulation = 0.5f + 0.4f * static_cast<float>(std::sin(ImGui::GetTime() * 2.0));
    knob("##cutoff", &cutoff, 20.0f, 20000.0f, 1200.0f, cutoffStyle);
    ImGui::SameLine();
    KnobStyle voiceStyle { 34.0f, palette().warning, "Voices", "%.0f" };
    voiceStyle.flags = knobInteger;
    voiceStyle.steps = 15;
    knob("##voices", &voices, 1.0f, 16.0f, 8.0f, voiceStyle);

    static float faderDb = 0.0f;
    fader("##fader", &faderDb, -70.0f, 6.0f, dp(30.0f, 150.0f));
    ImGui::SameLine();
    const auto level = 0.35f + 0.3f * std::abs(std::sin(t * 1.7f));
    meter("##meter", dp(12.0f, 150.0f), level * 1.2f, level, level * 0.7f, level * 0.6f);
    ImGui::SameLine();
#endif
    ImGui::BeginGroup();
    static float opacity = 0.75f, tempo = 120.0f;
    sliderBar("##opacity", &opacity, 0.0f, 1.0f, "Opacity %.2f", dp(220.0f));
    dragValue("##tempo", &tempo, 0.1f, 20.0f, 300.0f, "%.2f BPM", dp(220.0f));
#if defined(CYUI_HAS_AUDIO)
    meter("##hmeter", ImVec2(dp(220.0f), dp(8.0f)), level, level * 0.9f, level * 0.6f, level * 0.5f, false);
#endif
    space(4.0f);
    progressBar(std::fmod(t * 0.15f, 1.0f), ImVec2(dp(220.0f), 0.0f));
    progressBar(-1.0f, ImVec2(dp(220.0f), 0.0f), nullptr, Tone::info);
    progressBar(0.62f, ImVec2(dp(220.0f), 0.0f), "Exporting 62 %", Tone::success);
    spinner("##spin");
    ImGui::SameLine();
    textDim("Working...");
    ImGui::EndGroup();

    space();
    sectionTitle("Fields");
    static char search[128] {};
    searchField("##search", search, sizeof(search), "Search effects", dp(260.0f));
    static char name[128] { "Layer 1" };
    textField("##name", name, sizeof(name), "Name", dp(260.0f));
    static auto blend = 0;
    const char* blends[] { "Normal", "Multiply", "Screen", "Overlay" };
    combo("##blend", &blend, blends, 4, dp(260.0f));
}

void containersPage()
{
    if (beginCard("##card1", "Project")) {
        textDim("D:/Projects/MySong.cydaw");
        ImGui::SameLine();
        badge("SAVED", Tone::success);
        space(4.0f);
        primaryButton("Open");
        ImGui::SameLine();
        button("Reveal");
    }
    endCard();
    space(6.0f);
    if (beginCard("##card2", "Update available", 0.0f, Tone::accent)) {
        ImGui::TextWrapped("Version 2.4 brings a new mixer and faster loading.");
        button("Install", Variant::primary);
    }
    endCard();

    space();
    if (beginSection("Transform")) {
        static float position[2] { 120.0f, 64.0f };
        static float rotation = 0.0f;
        static auto visible = true;
        static auto locked = false;
        if (beginProperties("##transform")) {
            property("Position");
            ImGui::DragFloat2("##position", position);
            property("Rotation", "Degrees, clockwise.");
            sliderBar("##rotation", &rotation, -180.0f, 180.0f, "%.1f deg", -1.0f);
            property("Visible");
            toggleSwitch("##visible", &visible);
            property("Locked");
            ImGui::Checkbox("##locked", &locked);
            property("A very long property label that is clipped");
            static auto mode = 0;
            const char* modes[] { "Fast", "Best" };
            segmented("##mode", modes, 2, &mode, -1.0f);
            endProperties();
        }
        endSection();
    }
    if (beginSection("Appearance", false)) {
        static float colour[3] { 0.9f, 0.5f, 0.2f };
        if (beginProperties("##appearance")) {
            property("Colour");
            ImGui::ColorEdit3("##colour", colour);
            endProperties();
        }
        endSection();
    }

    space();
    sectionTitle("Table (stock ImGui, themed)");
    if (ImGui::BeginTable("##table", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Size");
        ImGui::TableHeadersRow();
        const char* rows[][3] { { "kick.wav", "Audio", "312 kB" }, { "lead.mid", "MIDI", "4 kB" }, { "mix.cydaw", "Project", "6.2 MB" } };
        for (const auto& row : rows) {
            ImGui::TableNextRow();
            for (auto c = 0; c < 3; ++c) {
                ImGui::TableSetColumnIndex(c);
                if (c == 0)
                    ImGui::TextUnformatted(row[c]);
                else
                    textDim("%s", row[c]);
            }
        }
        ImGui::EndTable();
    }

    space();
    static auto showModal = false;
    if (button("Open a modal"))
        showModal = true;
    ImGui::SameLine();
    if (button("Notify"))
        notify("Project saved", Tone::success);
    ImGui::SameLine();
    if (button("Notify error"))
        notify("Could not open the audio device", Tone::error);
    if (beginModal("Delete the layer?", &showModal)) {
        ImGui::TextWrapped("The layer \"Background\" and its masks will be deleted. This cannot be undone.");
        if (modalButtons("Delete", "Cancel", true) == 1)
            notify("Layer deleted", Tone::neutral);
        endModal();
    }

    space();
    if (beginCard("##empty", nullptr, 170.0f))
        emptyState("No layers yet", "Drop an image here, or press Ctrl+N", icon::photo);
    endCard();
}

// The application shell at a small scale. The real top bar, status bar and
// sidebar are viewport bars (the demo application around this window shows
// them); child regions stand in for them here.
void layoutPage()
{
    const auto& p = palette();
    callout(Tone::info, "beginTopBar, beginStatusBar and beginSidebar take their room off the main viewport, then dockSpace "
                        "fills the rest. Below, child regions stand in for those bars.");
    static auto mode = 0;
    static auto page = 0;
    static auto playing = false;

    space();
    sectionTitle("Top bar");
    {
        ScopedColor ground(ImGuiCol_ChildBg, p.bg1);
        ScopedVar padding(ImGuiStyleVar_WindowPadding, dp(8.0f, metrics().toolbarHeight * 0.5f));
        if (ImGui::BeginChild("##topBarPreview", ImVec2(0.0f, ImGui::GetFrameHeight() + dp(metrics().toolbarHeight)),
                              ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar)) {
            const char* modes[] { "Studio", "Mixer", "Library" };
            segmented("##mode", modes, 3, &mode);
            toolbarSeparator();
            if (iconButton(playing ? icon::pause : icon::play, "Play / pause (Space)", playing, p.success))
                playing = !playing;
            ImGui::SameLine(0.0f, dp(2.0f));
            iconButton(icon::stop, "Stop");
        }
        ImGui::EndChild();
    }

    space();
    sectionTitle("Sidebar, panel header and toolbar");
    const auto height = dp(250.0f);
    {
        ScopedColor ground(ImGuiCol_ChildBg, p.bg0);
        ScopedVar padding(ImGuiStyleVar_WindowPadding, dp(10.0f, 12.0f));
        if (ImGui::BeginChild("##sidebarPreview", ImVec2(dp(190.0f), height),
                              ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding)) {
            sidebarHeader("CYUI DEMO", "1.1");
            sidebarGroup("WORKSPACE");
            if (sidebarItem(icon::home, "Home", page == 0))
                page = 0;
            if (sidebarItem(icon::library, "Library", page == 1, "12"))
                page = 1;
            sidebarGroup("SYSTEM");
            if (sidebarItem(icon::settings, "Settings", page == 2))
                page = 2;
        }
        ImGui::EndChild();
    }
    ImGui::SameLine();
    if (ImGui::BeginChild("##panelPreview", ImVec2(0.0f, height), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding)) {
        panelHeader("Devices", "Kick drum - Track 1");
        space(4.0f);
        if (beginToolbar("##toolbarPreview")) {
            iconButton(icon::add, "Add a device");
            ImGui::SameLine(0.0f, dp(2.0f));
            iconButton(icon::presets, "Presets");
            toolbarSeparator();
            badge("3 DEVICES");
        }
        endToolbar();
        space(6.0f);
        textDim("Panel content: cards, properties, views.");
    }
    ImGui::EndChild();

    space();
    sectionTitle("Status bar");
    {
        ScopedColor ground(ImGuiCol_ChildBg, p.bg0);
        ScopedVar padding(ImGuiStyleVar_WindowPadding, dp(8.0f, 4.0f));
        if (ImGui::BeginChild("##statusPreview", ImVec2(0.0f, ImGui::GetFontSize() * 0.86f + dp(10.0f) + dp(2.0f)),
                              ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar)) {
            statusText(Tone::neutral, "Ready");
            ImGui::SameLine(0.0f, dp(18.0f));
            statusText(Tone::success, "Audio: 48 kHz, 256 samples");
            ImGui::SameLine(0.0f, dp(18.0f));
            statusText(Tone::warning, "2 warnings");
        }
        ImGui::EndChild();
    }

    space();
    sectionTitle("Dock space");
#ifdef IMGUI_HAS_DOCK
    textDim("dockSpace() docks the panels over what the bars leave of the main viewport.");
#else
    textFaint("dockSpace() needs ImGui's docking branch.");
#endif
}

#if defined(CYUI_HAS_PLOTS)
void plotsPage()
{
    sectionTitle("Frequency response");
    textDim("Logarithmic frequency axis, multiple series and host-defined units.");
    static std::vector<Point> low, bell, combined;
    if (low.empty()) {
        for (int i = 0; i < 360; ++i) {
            const double hz = 20 * std::pow(1000., i / 359.);
            const double shelf = 5.5 / (1 + std::pow(hz / 180., 3));
            const double peak = -7 * std::exp(-std::pow(std::log(hz / 1800.) / .42, 2));
            low.push_back({hz, shelf});
            bell.push_back({hz, peak});
            combined.push_back({hz, shelf + peak + 3 * std::exp(-std::pow(std::log(hz / 8500.) / .7, 2))});
        }
    }
    static PlotState response{{{20, 20000}, true}, {{-12, 12}, false}};
    PlotSeries series[]{{1, "Low shelf", low, PlotStyle::line, 1},
                        {2, "Bell", bell, PlotStyle::line, 2},
                        {3, "Combined response", combined, PlotStyle::line, 0}};
    PlotOptions options;
    options.formatX = [](double hz) {
        char label[32];
        std::snprintf(label, sizeof(label), hz >= 1000 ? "%.1f kHz" : "%.0f Hz", hz >= 1000 ? hz / 1000 : hz);
        return std::string(label);
    };
    options.formatY = [](double db) {
        char label[32];
        std::snprintf(label, sizeof(label), "%+.0f dB", db);
        return std::string(label);
    };
    plot("##response", response, {series, 3}, dp(0, 260), options);
    space();
    if (ImGui::BeginTable("##plotDetails", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        sectionTitle("Distribution");
        static std::vector<Point> bins = [] {
            std::vector<double> samples;
            for (int i = 0; i < 1200; ++i)
                samples.push_back(std::sin(i * .13) * std::sin(i * .037));
            const auto counts = histogram(samples, {-1, 1}, 32);
            std::vector<Point> result;
            for (std::size_t i = 0; i < counts.size(); ++i)
                result.push_back({double(i), double(counts[i])});
            return result;
        }();
        static PlotState histogramView{{{0, 32}, false}, {{0, 160}, false}};
        PlotSeries distribution{4, "Samples", bins, PlotStyle::bars, 1};
        plot("##distribution", histogramView, {&distribution, 1}, dp(0, 190));
        ImGui::TableNextColumn();
        sectionTitle("Heatmap");
        static std::vector<float> values = [] {
            std::vector<float> result(32 * 16);
            for (int y = 0; y < 16; ++y)
                for (int x = 0; x < 32; ++x)
                    result[std::size_t(y * 32 + x)] = float(.5 + .5 * std::sin(x * .25) * std::cos(y * .3));
            return result;
        }();
        heatmap("##heatmap", values, 32, 16, {0, 1}, dp(0, 190));
        ImGui::EndTable();
    }
}
#endif

#if defined(CYUI_HAS_NODES)
void nodesPage()
{
    sectionTitle("Voice graph");
    textDim("Drag nodes or connect pins. Ctrl: multi-select. Home: frame. Delete: remove. Execution is host-owned.");
    static NodeState state;
    static std::vector<Node> nodes{
        {1, {30, 35}, "On note", {}, {{11, "Trigger", 3}, {12, "Pitch", 1}, {13, "Velocity", 2}}, 3},
        {2, {30, 270}, "Voice settings", {}, {{21, "Cutoff", 0}, {22, "Resonance", 0}, {23, "Level", 2}}, 0},
        {3, {330, 35}, "Oscillator", {{31, "Trigger", 3}, {32, "Pitch", 1}, {33, "Velocity", 2}}, {{34, "Audio", 0}}, 1},
        {4, {640, 55}, "Filter", {{41, "Audio", 0}, {42, "Cutoff", 0}, {43, "Resonance", 0}}, {{44, "Signal", 0}}, 0},
        {5, {640, 290}, "Gain", {{51, "Signal", 0}, {52, "Level", 2}}, {{53, "Output", 0}}, 2},
        {6, {950, 190}, "Stereo output", {{61, "Left", 0}, {62, "Right", 0}}, {}, 3}};
    static std::vector<NodeLink> links{{1, 11, 31, 3}, {2, 12, 32, 1}, {3, 13, 33, 2},
                                      {4, 34, 41, 0}, {5, 21, 42, 0}, {6, 22, 43, 0},
                                      {7, 44, 51, 0}, {8, 23, 52, 2}, {9, 53, 61, 0}, {10, 53, 62, 0}};
    static ItemId nextLink = 11;
    const auto size = dp(0, 470);
    static bool framed = false;
    if (!framed) {
        state.canvas.view.fit({0, 0}, {1170, 425}, {ImGui::GetContentRegionAvail().x, size.y}, dp(24));
        framed = true;
    }
    if (ghostButton("Arrange nodes"))
        for (const auto &move : arrangeNodes(nodes, links, {290, 145}))
            for (auto &node : nodes)
                if (node.id == move.id)
                    node.position = move.position;
    NodeOptions options;
    options.snap = 8;
    const auto edit = nodeEditor("##nodes", state, nodes, links, size, options);
    if (edit.moved.committed())
        for (const auto &move : edit.moved.value)
            for (auto &node : nodes)
                if (node.id == move.id)
                    node.position = move.position;
    if (edit.connect)
        links.push_back({nextLink++, edit.connectFrom, edit.connectTo, 0});
    if (edit.linkRemoved)
        links.erase(std::remove_if(links.begin(), links.end(), [&](const NodeLink &link) {
            return link.id == edit.removeLink;
        }), links.end());
    for (auto id : edit.removeNodes) {
        for (const auto &node : nodes)
            if (node.id == id)
                links.erase(std::remove_if(links.begin(), links.end(), [&](const NodeLink &link) {
                    const auto owns = [&](ItemId pin) {
                        return std::any_of(node.inputs.begin(), node.inputs.end(), [&](const NodePin &p) { return p.id == pin; }) ||
                               std::any_of(node.outputs.begin(), node.outputs.end(), [&](const NodePin &p) { return p.id == pin; });
                    };
                    return owns(link.from) || owns(link.to);
                }), links.end());
        nodes.erase(std::remove_if(nodes.begin(), nodes.end(), [&](const Node &node) { return node.id == id; }), nodes.end());
    }
}
#endif

#if defined(CYUI_HAS_AUDIOVIEWS)
void audioViewsPage()
{
    sectionTitle("Stereo waveform");
    static std::vector<Peak> left, right;
    if (left.empty()) {
        std::vector<float> a, b;
        for (int i = 0; i < 48000; ++i) {
            const double envelope = .15 + .7 * std::abs(std::sin(i * .00015)) * std::exp(-i / 90000.);
            a.push_back(float(envelope * (std::sin(i * .045) + .2 * std::sin(i * .12))));
            b.push_back(float(envelope * (std::sin(i * .045 + .7) + .2 * std::sin(i * .13))));
        }
        left = makePeaks(a, 1600);
        right = makePeaks(b, 1600);
    }
    static WaveformState wave;
    WaveformData data{{0, 1}, {left, right}};
    waveform("##wave", wave, data, .42, dp(0, 150));
    if (ImGui::BeginTable("##audioMonitors", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        sectionTitle("Spectrum");
        static std::vector<Point> input, output, signal, stereo;
        if (input.empty()) {
            for (int i = 0; i < 320; ++i) {
                const double hz = 20 * std::pow(1000., i / 319.);
                const double level = -65 + 33 * std::exp(-std::pow(std::log(hz / 1100.) / 1.5, 2)) +
                                     3 * std::sin(i * .2) + 1.5 * std::cos(i * .53);
                input.push_back({hz, level});
                output.push_back({hz, level - 6 * std::exp(-std::pow(std::log(hz / 1800.) / .4, 2))});
                signal.push_back({i / 319., .7 * std::sin(i * .055) + .18 * std::sin(i * .11)});
                stereo.push_back({.8 * std::sin(i * .065), .65 * std::sin(i * .065 + .65)});
            }
        }
        static PlotState frequencies{{{20, 20000}, true}, {{-90, 0}, false}};
        PlotSeries spectrumSeries[]{{1, "Input", input, PlotStyle::line, 1}, {2, "Output", output, PlotStyle::line, 0}};
        spectrum("##spectrum", frequencies, {spectrumSeries, 2}, dp(0, 185));
        ImGui::TableNextColumn();
        sectionTitle("Oscilloscope");
        static PlotState scope{{{0, 1}, false}, {{-1, 1}, false}};
        PlotSeries trace{3, "Signal", signal, PlotStyle::line, 2};
        oscilloscope("##oscilloscope", scope, {&trace, 1}, dp(0, 185));
        ImGui::TableNextColumn();
        sectionTitle("Stereo scope");
        static PlotState field{{{-1, 1}, false}, {{-1, 1}, false}};
        PlotSeries stereoSeries{4, "L / R", stereo, PlotStyle::points, 3};
        stereoScope("##stereo", field, {&stereoSeries, 1}, dp(0, 170));
        ImGui::TableNextColumn();
        sectionTitle("Monitor controls");
#if defined(CYUI_HAS_AUDIO)
        static float gain = .65f, pan = -.15f, cutoff = 1800;
        knob("##monitorGain", &gain, 0, 1, .65f, {42, 0, "Gain"});
        ImGui::SameLine(0, dp(24));
        KnobStyle panStyle{42, palette().info, "Pan"};
        panStyle.flags = knobBipolar;
        knob("##monitorPan", &pan, -1, 1, 0, panStyle);
        ImGui::SameLine(0, dp(24));
        KnobStyle filterStyle{42, palette().success, "Cutoff", "%.0f", "Hz"};
        filterStyle.flags = knobLogarithmic;
        knob("##monitorCutoff", &cutoff, 20, 20000, 1800, filterStyle);
        meter("##monitorLevel", {ImGui::GetContentRegionAvail().x, dp(24)}, .75f, .62f, .42f, .38f, false);
#endif
        textDim("Synthetic preview data. Processing and playback are supplied by the host.");
        ImGui::EndTable();
    }
}
#endif

#if defined(CYUI_HAS_CODE)
void codePage()
{
    sectionTitle("Source view & annotations");
    static CodeDocument document;
    static CodeViewState state;
    document.setText("// audio/Voice.cpp - read-only source with host annotations\n"
                     "#include <algorithm>\n\n"
                     "float applyGain(float sample, float gainDb) {\n"
                     "    const float gain = dbToLinear(gainDb);\n"
                     "    return std::clamp(sample * gain, -1.0f, 1.0f);\n"
                     "}\n\n"
                     "// Source data, diagnostics and file access belong to the host.\n", CodeLanguage::cpp);
    CodeAnnotation note{1, 5, Tone::info, "Host-supplied diagnostic: inspect gain conversion"};
    codeViewer("##source", state, document, dp(0, 205), {&note, 1});
#if defined(CYUI_HAS_CODEEDITOR)
    space();
    sectionTitle("Native code editor");
    textDim("Edit this document. Selection, Undo/Redo and language colors use the native editor backend.");
    static CodeEditor editor;
    static bool initialized = false;
    if (!initialized) {
        editor.setLanguage(CodeLanguage::cpp);
        editor.native().SetShowWhitespacesEnabled(false);
        editor.setText("// ui/Monitor.cpp - editable C++\n"
                       "#include <cyui/Core.h>\n"
                       "#include <cyui/modules/Audio.h>\n\n"
                       "void drawMonitor(float& gain) {\n"
                       "    cyui::sectionTitle(\"Output monitor\");\n"
                       "    cyui::KnobStyle style{42.0f, 0, \"Gain\"};\n"
                       "    if (cyui::knob(\"##gain\", &gain, 0.0f, 1.0f, 0.5f, style)) {\n"
                       "        // Apply the host's preview or Undo transaction here.\n"
                       "        updatePreview(gain);\n"
                       "    }\n"
                       "}\n");
        initialized = true;
    }
    editor.draw("##editable", dp(0, 275));
#endif
}
#endif

#if defined(CYUI_HAS_DIFF)
void diffPage()
{
    sectionTitle("Compare document revisions");
    static DiffDocument document;
    static DiffState state;
    static bool initialized = false;
    if (!initialized) {
        document.setTexts(
            "#include <algorithm>\n\n"
            "struct Monitor {\n"
            "    float gain = 0.5f;\n"
            "    bool enabled = true;\n"
            "};\n\n"
            "float process(float sample, const Monitor& monitor) {\n"
            "    if (!monitor.enabled) {\n"
            "        return sample;\n"
            "    }\n"
            "    return sample * monitor.gain;\n"
            "}\n",
            "#include <algorithm>\n"
            "#include <cmath>\n\n"
            "struct Monitor {\n"
            "    float gainDb = -6.0f;\n"
            "    bool enabled = true;\n"
            "    bool limitOutput = true;\n"
            "};\n\n"
            "float process(float sample, const Monitor& monitor) {\n"
            "    if (!monitor.enabled) {\n"
            "        return sample;\n"
            "    }\n"
            "    const float gain = std::pow(10.0f, monitor.gainDb / 20.0f);\n"
            "    const float output = sample * gain;\n"
            "    return monitor.limitOutput\n"
            "        ? std::clamp(output, -1.0f, 1.0f) : output;\n"
            "}\n", CodeLanguage::cpp);
        initialized = true;
    }
    diffViewer("##diff", state, document, dp(0, 440));
}
#endif

void moduleGallery(int selectedTab,const GalleryResources& resources) {
    (void)resources;
    auto page=[&](const char*label,int index,auto body){if(ImGui::BeginTabItem(label,nullptr,selectedTab==index?ImGuiTabItemFlags_SetSelected:0)){body();ImGui::EndTabItem();}};
#if defined(CYUI_HAS_CANVAS)
    page("Canvas",4,[]{textDim("Wheel: zoom at pointer. Middle drag: pan. Space + drag: pan.");static CanvasState state;CanvasScope canvas("##canvas",state,dp(0,320));const auto&f=canvas.frame();auto*d=ImGui::GetWindowDrawList();const auto a=state.view.screen({40,40},f.bounds.min),b=state.view.screen({220,160},f.bounds.min);d->AddRectFilled(a,b,palette().accentDim,dp(4));d->AddText(a,palette().text,"World-space content");});
#endif
#if defined(CYUI_HAS_PLOTS)
    page("Plots",5,plotsPage);
#endif
#if defined(CYUI_HAS_CURVES)
    page("Curves",6,[]{textDim("Double-click: add/remove. Drag: edit. Ctrl: multi-select. Escape: cancel.");static std::vector<CurvePoint>points{{1,0,.2},{2,2,.8,CurveShape::smooth},{3,5,.3,CurveShape::bezier,{-1,.1},{1,.2}},{4,8,.7}};static CurveState state;static bool init=false;if(!init){state.view.x.range={0,10};state.view.y.range={0,1};init=true;}CurveOptions options;options.snap=.25;const auto edit=curveEditor("##curves",state,points,dp(0,320),options);if(edit.committed())points=edit.value;});
#endif
#if defined(CYUI_HAS_NODES)
    page("Nodes",7,nodesPage);
#endif
#if defined(CYUI_HAS_TIMELINE)
    page("Timeline",8,[]{textDim("Wheel: zoom. Shift-wheel: pan. Ctrl-wheel: scroll lanes. Drag clip/edges to move/trim.");static TimelineState state;static std::vector<TimelineLane>lanes{{1,"Video",70},{2,"Audio",80},{3,"Events",48}};static std::vector<TimelineClip>clips{{1,1,1,3,"Opening",0},{2,1,5,3,"Sequence",1},{3,2,.5,7,"Sound",2},{4,3,2,1,"Marker interval",3}};TimelineMarker marker{1,4,"Cue"};TimelineOptions o;o.playhead=3.5;o.snap=.25;auto e=timeline("##timeline",state,lanes,clips,{&marker,1},dp(0,350),o);if(e.clips.committed())clips=e.clips.value;});
#endif
#if defined(CYUI_HAS_AUDIOVIEWS)
    page("Audio views",9,audioViewsPage);
#endif
#if defined(CYUI_HAS_CODE)
    page("Code",10,codePage);
#endif
#if defined(CYUI_HAS_DIFF)
    page("Code / Diff",11,diffPage);
#endif
#if defined(CYUI_HAS_INSPECTOR)
    page("Inspector",12,[]{static InspectorState state;static std::vector<Property>fields=[](){std::vector<Property>v;Property a;a.id=1;a.label="Enabled";a.value=true;v.push_back(a);a={};a.id=2;a.label="Exact integer";a.help="64-bit value; no conversion through double.";a.value=std::int64_t(9007199254740993LL);v.push_back(a);a={};a.id=3;a.label="Gain";a.value=.5;a.bounded=true;a.minimum=0;a.maximum=1;v.push_back(a);a={};a.id=4;a.label="Name";a.value=std::string("Example");v.push_back(a);a={};a.id=5;a.label="Mode";a.value=std::string("Preview");a.choices={"Preview","Final"};v.push_back(a);return v;}();for(const auto&e:inspector("##inspector",state,fields))if(e.edit.committed())for(auto&f:fields)if(f.id==e.id)f.value=e.edit.value;});
#endif
#if defined(CYUI_HAS_ASSETS)
    page("Assets",13,[]{static AssetState state;static std::vector<AssetItem>items{{1,0,"Images","Folder",0,true},{2,0,"Sounds","Folder",0,true},{3,0,"Example material","Material"},{4,1,"Landscape","Image"},{5,2,"Atmosphere","Audio"}};assetBrowser("##browser",state,items,dp(0,360));});
#endif
#if defined(CYUI_HAS_IMAGES)
    page("Images",14,[&]{static GradientState gradient;static std::vector<GradientStop>stops; if(stops.empty())stops={{1,0,vec(palette().info)},{2,.5,vec(palette().accent)},{3,1,vec(palette().success)}};auto e=gradientEditor("##gradient",gradient,stops,dp(0,56));if(e.committed())stops=e.value;static ImageState state;ImageData image{resources.texture,resources.width?resources.width:256,resources.height?resources.height:256};imageViewport("##image",state,image,dp(0,330),ImageTool::crop);});
#endif
#if defined(CYUI_HAS_VIEWPORT)
    page("Viewport",15,[&]{textDim("Alt-left drag: orbit. Middle drag: pan. Wheel: dolly. Renderer and scene remain host-owned.");static ViewportState state;ImageData image{resources.texture,resources.width?resources.width:256,resources.height?resources.height:256};viewport("##viewport",state,image,dp(0,220));textDim("2D transform overlay: move center, scale corners, rotate top handle.");{CanvasState canvas;CanvasOptions o;o.grid=true;o.navigation=false;CanvasScope scope("##transform",canvas,dp(0,200),o);static TransformState transformState;static Transform2D value{{150,90},{150,70},.2};TransformOptions options;const auto origin=scope.frame().bounds.min;options.project=[=](Point p){return ImVec2(origin.x+float(p.x),origin.y+float(p.y));};options.unproject=[=](ImVec2 p){return Point{p.x-origin.x,p.y-origin.y};};auto edit=transformGizmo("##gizmo",transformState,value,options);if(edit.committed())value=edit.value;}
#if defined(CYUI_HAS_IMGUIZMO)
        {CanvasState three;CanvasScope area("##3d",three,dp(0,200));static Gizmo3DState gizmo;static std::array<float,16>matrix{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};OrbitCamera camera;const auto view=cameraViewMatrix(camera);const std::array<float,16>projection{1.5f,0,0,0,0,2,0,0,0,0,-1.002f,-1,0,0,-.2002f,0};const auto edit3d=gizmo3D("##scene",gizmo,matrix,view,projection,area.frame().bounds);if(edit3d.committed())matrix=edit3d.value;}
#endif
        });
#endif
#if defined(CYUI_HAS_COMMANDS)
    page("Commands",16,[&]{static bool open=false;static CommandState state;static std::vector<Command>commands{{1,"Open document","Ctrl+O","File"},{2,"Save document","Ctrl+S","File"},{3,"Frame selection","Home","View"},{4,"Unavailable command","","Example",false}};if(primaryButton("Open command palette")||resources.previewCommands)open=true;const auto action=commandPalette("Commands###examplePalette",open,state,commands);if(action)notify("Command requested (handled by the host)");});
#endif
#if defined(CYUI_HAS_DIAGNOSTICS)
    page("Diagnostics",17,[]{static LogBuffer buffer;static LogState state;static bool init=false;if(!init){buffer.append(LogLevel::info,"Application initialized");buffer.append(LogLevel::warning,"Example warning");buffer.append(LogLevel::error,"Example diagnostic — no file is modified");init=true;}logView("##log",state,buffer,dp(0,210));std::vector<Diagnostic>rows{{1,LogLevel::warning,"example.cpp","Unused value",8,4},{2,LogLevel::error,"shader.hlsl","Example compiler diagnostic",12,3}};diagnosticList("##diagnostics",rows,dp(0,140));});
#endif
#if defined(CYUI_HAS_DOCUMENTVIEWS)
    page("Documents",18,[]{static Document doc;static DocumentState state;doc.setMarkdown("# Shared documents\nA **formatted** paragraph with `inline code` and a [host-handled link](https://example.com).\n- Reusable document presentation\n> Notes use the active palette.\n| Module | Content |\n|---|---|\n| Documents | Markdown |\n| Code | Source and diff |\n```cpp\nfloat gain = 0.5f;\n```\n");documentView("##document",state,doc,dp(0,300));static TranscriptState chat;std::vector<TranscriptMessage>messages{{1,"User","Show the result.","10:00"},{2,"Assistant","The **result** is ready.\n```json\n{\"status\": \"ready\"}\n```","10:01"}};transcript("##chat",chat,messages,dp(0,260));});
#endif
#if defined(CYUI_HAS_WEB)
    page("Web",19,[&]{textDim("Optional CEF rendering in an ImGui texture. HTML/compiled TypeScript; host-owned messages.");
        struct Preview:WebBackend{void place(const WebPlacement&)override{}bool navigate(const std::string&)override{return false;}bool postJson(const std::string&)override{return false;}bool poll(WebEvent&)override{return false;}bool ready()const override{return false;}};static Preview preview;
        if(!resources.web)callout(Tone::info,"Gallery preview: attach a CyUI::WebCEF backend in GalleryResources.web to load an interactive page.");
        WebBackend&backend=resources.web?*resources.web:preview;static std::string lastWebTheme;const auto currentWebTheme=webThemeJson();if(currentWebTheme!=lastWebTheme&&backend.postJson(currentWebTheme))lastWebTheme=currentWebTheme;
        for(const auto&e:webPanel("##web",backend,dp(0,340))){if(e.kind==WebEventKind::navigated)backend.postJson(webThemeJson());if(e.kind==WebEventKind::message&&(e.data.find("page.ready")!=std::string::npos||e.data.find("chat.message")!=std::string::npos))backend.postJson("{\"protocol\":\"cyui.web\",\"version\":1,\"type\":\"host.reply\",\"payload\":{\"status\":\"Message received by CyUI\"}}");if(e.kind==WebEventKind::error)textColoured(palette().error,"%s",e.data.c_str());}
    });
#endif

}

}  // namespace

void showGallery(bool* open, const int selectTab) { showGallery(open,selectTab,GalleryResources{}); }

void showGallery(bool* open, const int selectTab,const GalleryResources& resources)
{
    ImGui::SetNextWindowSize(dp(760.0f, 680.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("CyUI Gallery", open)) {
        ImGui::End();
        return;
    }
    themeBar();
    palettePreview();
    divider(2.0f);
    if (ImGui::BeginTabBar("##galleryTabs",ImGuiTabBarFlags_FittingPolicyScroll)) {
        // Brings selectTab to the front once (and again when it changes): the
        // tabs stay free to click.
        auto* storage = ImGui::GetStateStorage();
        const auto key = ImGui::GetID("##selectedTab");
        const auto selected = selectTab != storage->GetInt(key, -1) ? selectTab : -1;
        storage->SetInt(key, selectTab);
        if (ImGui::BeginTabItem("Text & status", nullptr, selected == 0 ? ImGuiTabItemFlags_SetSelected : 0)) {
            textPage();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Buttons", nullptr, selected == 1 ? ImGuiTabItemFlags_SetSelected : 0)) {
            buttonsPage();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Controls", nullptr, selected == 2 ? ImGuiTabItemFlags_SetSelected : 0)) {
            controlsPage();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Containers", nullptr, selected == 3 ? ImGuiTabItemFlags_SetSelected : 0)) {
            containersPage();
            ImGui::EndTabItem();
        }
        moduleGallery(selected,resources);
        if (ImGui::BeginTabItem("Layout", nullptr, selected == 20 ? ImGuiTabItemFlags_SetSelected : 0)) {
            layoutPage();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void showThemeEditor(bool* open)
{
    ImGui::SetNextWindowSize(dp(420.0f, 560.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("CyUI Theme editor", open)) {
        ImGui::End();
        return;
    }
    themeBar();
    static char path[260] { "my_theme.cytheme" };
    textField("##path", path, sizeof(path), "File", -dp(120.0f));
    ImGui::SameLine();
    if (button("Save")) {
        if (savePalette(path, palette()))
            notify("Theme saved", Tone::success);
        else
            notify("Could not save the theme", Tone::error);
    }
    ImGui::SameLine();
    if (button("Load")) {
        auto loaded = palette();
        if (loadPalette(path, loaded)) {
            setPalette(loaded);
            notify("Theme loaded", Tone::success);
        } else {
            notify("Could not read the theme", Tone::error);
        }
    }
    static const char* groups[] { "Grounds", "Lines", "Text", "Accent", "Meaning" };
    auto edited = palette();
    auto changed = false;
    for (auto group = 0; group < 5; ++group) {
        if (!beginSection(groups[group]))
            continue;
        if (beginProperties(groups[group])) {
            for (const auto& field : paletteFields()) {
                if (field.group != group)
                    continue;
                property(field.label);
                auto colour = vec(edited.*field.member);
                ImGui::PushID(field.key);
                if (ImGui::ColorEdit4("##c", &colour.x, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayHex)) {
                    edited.*field.member = ImGui::ColorConvertFloat4ToU32(colour);
                    changed = true;
                }
                ImGui::PopID();
            }
            endProperties();
        }
        endSection();
    }
    if (changed)
        setPalette(edited);
    ImGui::End();
}

}  // namespace cyui
