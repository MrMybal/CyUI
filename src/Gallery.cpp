#include "cyui/Gallery.h"

#include "cyui/Icons.h"
#include "cyui/Layout.h"
#include "cyui/Theme.h"
#include "cyui/CoreWidgets.h"

#if defined(CYUI_HAS_AUDIO)
#include "cyui/modules/Audio.h"
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


void moduleGallery(int selectedTab,const GalleryResources& resources) {
    auto page=[&](const char*label,int index,auto body){if(ImGui::BeginTabItem(label,nullptr,selectedTab==index?ImGuiTabItemFlags_SetSelected:0)){body();ImGui::EndTabItem();}};
#if defined(CYUI_HAS_CANVAS)
    page("Canvas",4,[]{textDim("Wheel: zoom at pointer. Middle drag: pan. Space + drag: pan.");static CanvasState state;CanvasScope canvas("##canvas",state,dp(0,320));const auto&f=canvas.frame();auto*d=ImGui::GetWindowDrawList();const auto a=state.view.screen({40,40},f.bounds.min),b=state.view.screen({220,160},f.bounds.min);d->AddRectFilled(a,b,palette().accentDim,dp(4));d->AddText(a,palette().text,"World-space content");});
#endif
#if defined(CYUI_HAS_PLOTS)
    page("Plots",5,[]{static std::vector<Point>points; if(points.empty())for(int i=0;i<100;++i)points.push_back({i/10.,std::sin(i/10.)});static PlotState state{{{0,10},false},{{-1.2,1.2},false}};PlotSeries series{1,"Signal",points,PlotStyle::line,0};plot("##plot",state,{&series,1},dp(0,190));static std::vector<double>samples; if(samples.empty())for(int i=0;i<500;++i)samples.push_back(std::sin(i*.13));const auto bins=histogram(samples,{-1,1},24);std::vector<Point>bars;for(std::size_t i=0;i<bins.size();++i)bars.push_back({double(i),double(bins[i])});static PlotState barsView{{{0,24},false},{{0,75},false}};PlotSeries histogramSeries{2,"Distribution",bars,PlotStyle::bars,1};plot("##histogram",barsView,{&histogramSeries,1},dp(0,110));static std::vector<float>values(128);for(int i=0;i<128;++i)values[std::size_t(i)]=float((i%16)*(i/16));heatmap("##heatmap",values,16,8,{0,105},dp(0,100));});
#endif
#if defined(CYUI_HAS_CURVES)
    page("Curves",6,[]{textDim("Double-click: add/remove. Drag: edit. Ctrl: multi-select. Escape: cancel.");static std::vector<CurvePoint>points{{1,0,.2},{2,2,.8,CurveShape::smooth},{3,5,.3,CurveShape::bezier,{-1,.1},{1,.2}},{4,8,.7}};static CurveState state;static bool init=false;if(!init){state.view.x.range={0,10};state.view.y.range={0,1};init=true;}CurveOptions options;options.snap=.25;const auto edit=curveEditor("##curves",state,points,dp(0,320),options);if(edit.committed())points=edit.value;});
#endif
#if defined(CYUI_HAS_NODES)
    page("Nodes",7,[]{textDim("Drag cards or pins. Ctrl: multi-select. Home: frame. Delete requests removal.");static NodeState state;static std::vector<Node>nodes{{1,{30,40},"Source",{},{{11,"Output",0}},0},{2,{310,70},"Processor",{{21,"Input",0}},{{22,"Output",0}},1},{3,{590,140},"Output",{{31,"Input",0}},{},2}};static std::vector<NodeLink>links{{1,11,21,0},{2,22,31,1}};if(ghostButton("Arrange")){for(const auto&m:arrangeNodes(nodes,links))for(auto&n:nodes)if(n.id==m.id)n.position=m.position;}const auto e=nodeEditor("##nodes",state,nodes,links,dp(0,340));if(e.moved.committed())for(const auto&m:e.moved.value)for(auto&n:nodes)if(n.id==m.id)n.position=m.position;if(e.connectFrom)links.push_back({ItemId(links.size()+1),e.connectFrom,e.connectTo,0});if(e.removeLink)links.erase(std::remove_if(links.begin(),links.end(),[&](const NodeLink&l){return l.id==e.removeLink;}),links.end());for(auto id:e.removeNodes)nodes.erase(std::remove_if(nodes.begin(),nodes.end(),[&](const Node&n){return n.id==id;}),nodes.end());});
#endif
#if defined(CYUI_HAS_TIMELINE)
    page("Timeline",8,[]{textDim("Wheel: zoom. Shift-wheel: pan. Ctrl-wheel: scroll lanes. Drag clip/edges to move/trim.");static TimelineState state;static std::vector<TimelineLane>lanes{{1,"Video",70},{2,"Audio",80},{3,"Events",48}};static std::vector<TimelineClip>clips{{1,1,1,3,"Opening",0},{2,1,5,3,"Sequence",1},{3,2,.5,7,"Sound",2},{4,3,2,1,"Marker interval",3}};TimelineMarker marker{1,4,"Cue"};TimelineOptions o;o.playhead=3.5;o.snap=.25;auto e=timeline("##timeline",state,lanes,clips,{&marker,1},dp(0,350),o);if(e.clips.committed())clips=e.clips.value;});
#endif
#if defined(CYUI_HAS_AUDIOVIEWS)
    page("Audio views",9,[]{static std::vector<float>samples;static std::vector<Peak>peaks;if(samples.empty()){for(int i=0;i<48000;++i)samples.push_back(float(std::sin(i*.04)*std::sin(i*.0001)));peaks=makePeaks(samples,1200);}static WaveformState state;WaveformData data{{0,1},{peaks,peaks}};waveform("##wave",state,data,.4,dp(0,170));static std::vector<Point>frequencies;if(frequencies.empty())for(int i=0;i<160;++i){const double hz=20*std::pow(1000.,i/159.);frequencies.push_back({hz,-60+35*std::exp(-std::pow(std::log(hz/1000),2))});}static PlotState spectrumView{{{20,20000},true},{{-90,0},false}};PlotSeries series{1,"Spectrum",frequencies,PlotStyle::line,1};spectrum("##spectrum",spectrumView,{&series,1},dp(0,160));static PlotState scopeView{{{0,1},false},{{-1,1},false}};static std::vector<Point>signal;if(signal.empty())for(int i=0;i<300;++i)signal.push_back({i/300.,std::sin(i*.08)});PlotSeries scopeSeries{2,"Scope",signal,PlotStyle::line,2};oscilloscope("##oscilloscope",scopeView,{&scopeSeries,1},dp(0,110));static PlotState stereoView{{{-1,1},false},{{-1,1},false}};static std::vector<Point>stereo;if(stereo.empty())for(int i=0;i<180;++i)stereo.push_back({std::sin(i*.15),std::sin(i*.15+.8)});PlotSeries stereoSeries{3,"Stereo",stereo,PlotStyle::points,3};stereoScope("##stereo",stereoView,{&stereoSeries,1},dp(0,110));});
#endif
#if defined(CYUI_HAS_CODE)
    page("Code",10,[]{static CodeDocument doc;static CodeViewState state;doc.setText("// Shared source presentation\n#include <cyui/Core.h>\nint main() {\n\tconst char* label = u8\"Music — 音楽\";\n\treturn 42;\n}\n",CodeLanguage::cpp);CodeAnnotation note{1,4,Tone::info,"Example source annotation"};codeViewer("##source",state,doc,dp(0,210),{&note,1});
#if defined(CYUI_HAS_CODEEDITOR)
        static CodeEditor editor;static bool initialized=false;if(!initialized){editor.setLanguage(CodeLanguage::cpp);editor.setText("// Edit this document: native undo, selection and completion hooks.\nfloat gain = 0.5f;\n");initialized=true;}editor.draw("##editable",dp(0,220));
#endif
    });
#endif
#if defined(CYUI_HAS_DIFF)
    page("Code / Diff",11,[]{static DiffDocument doc;static DiffState state;static bool init=false;if(!init){doc.setTexts("// Example\nfloat gain = 0.5f;\nreturn gain;\n","// Example\nfloat gain = 0.8f;\nfloat pan = 0.0f;\nreturn gain;\n",CodeLanguage::cpp);init=true;}diffViewer("##diff",state,doc,dp(0,370));});
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
        if (ImGui::BeginTabItem("Text & status", nullptr, selectTab == 0 ? ImGuiTabItemFlags_SetSelected : 0)) {
            textPage();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Buttons", nullptr, selectTab == 1 ? ImGuiTabItemFlags_SetSelected : 0)) {
            buttonsPage();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Controls", nullptr, selectTab == 2 ? ImGuiTabItemFlags_SetSelected : 0)) {
            controlsPage();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Containers", nullptr, selectTab == 3 ? ImGuiTabItemFlags_SetSelected : 0)) {
            containersPage();
            ImGui::EndTabItem();
        }
        moduleGallery(selectTab,resources);
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
