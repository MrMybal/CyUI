#include <cyui/Core.h>
#include <imgui_internal.h>
#include <cstdio>

namespace {
int failures = 0;
void check(bool ok, const char *message) {
    if (!ok) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
}
template <class F>
void frame(F draw, ImVec2 mouse = {-100, -100}, int button = -1, bool down = false,
           ImGuiKey key = ImGuiKey_None) {
    auto &io = ImGui::GetIO();
    io.AddMousePosEvent(mouse.x, mouse.y);
    if (button >= 0)
        io.AddMouseButtonEvent(button, down);
    if (key != ImGuiKey_None)
        io.AddKeyEvent(key, true);
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({900, 650});
    ImGui::Begin("Test", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetCursorScreenPos({20, 20});
    draw();
    ImGui::Dummy({0, 0});
    ImGui::End();
    ImGui::Render();
    if (key != ImGuiKey_None)
        io.AddKeyEvent(key, false);
}
} // namespace
int main() {
    using namespace cyui;
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {900, 650};
    io.DeltaTime = 1.f / 60;
    apply();
    loadFonts();
    unsigned char *pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID(ImTextureID(1));
    TextFilter filter;
    check(!filter.active() && filter.matches("Anything"), "empty filter keeps all items");
    filter.query = " \t\n ";
    check(!filter.active(), "whitespace is not an active filter");
    check(foldFilterText("ÉTÉ Œuvre Straße e\xcc\x81 音楽") == "ete oeuvre strasse e 音楽",
          "Latin accents, ligatures and combining marks fold; other UTF-8 survives");
    filter.query = "CAST shadow";
    check(filter.matches("Cast shadows") && !filter.matches("Cast light"),
          "every word matches regardless of case");
    filter.query = "private-id";
    check(!filter.matches("Visible###private-id"), "hidden ImGui IDs are not search keywords");
    setLocale("fr");
    filter.query = "rugosite";
    check(filter.matches("Roughness"), "French accents are optional in translated labels");
    filter.query = "roughness";
    check(filter.matches("Roughness"), "English keys remain searchable in French");
    std::vector<FilterMenuItem> menu{
        {0,
         "File###private-id",
         {},
         {},
         true,
         false,
         {{0,
           "Export",
           {},
           {},
           true,
           false,
           {{0x100000001ull, "Export PNG", {}, "texture"}, {2, "Export HDR", {}, {}, false}}}}}};
    filter.query = "fichier png";
    auto matches = filterMenuMatches(menu, filter);
    check(matches.size() == 1 && matches[0].id == 0x100000001ull &&
              matches[0].breadcrumb == "Fichier / Exporter",
          "nested paths and translated action words combine without changing 64-bit IDs");
    filter.query = "export";
    matches = filterMenuMatches(menu, filter);
    check(matches.size() == 2 && !matches[1].enabled,
          "matching sections include all children and retain disabled actions");
    menu[0].enabled = false;
    check(!filterMenuMatches(menu, filter)[0].enabled, "disabled ancestors keep results disabled");
    menu[0].enabled = true;
    filter.query = "private-id";
    check(filterMenuMatches(menu, filter).empty(), "hidden ancestor IDs cannot match a menu");
    check(menu[0].label == "File###private-id", "search preserves the host menu model");
    setLocale("en");

    // Force matching collapsed sections open, then restore the saved preference.
    filter.query = "roughness";
    frame([&] {
        const auto id = ImGui::GetID(trLabel("Surface").c_str());
        ImGui::GetStateStorage()->SetInt(id, 0);
        check(beginFilteredSection("Surface", filter, true, false),
              "matching children reveal a collapsed section");
        endSection();
        check(ImGui::GetStateStorage()->GetInt(id, -1) == 0, "search does not overwrite section preferences");
        check(!beginFilteredSection("Identity", filter, false), "unrelated sections are hidden");
    });
    filter.clear();
    frame([&] {
        check(!beginFilteredSection("Surface", filter, true, false),
              "clearing restores the collapsed section");
    });

    // Real text entry exceeds small-string storage, then Escape clears the query.
    const auto input = [&] { filterBox("field", filter); };
    frame(input);
    filter.focus();
    frame(input);
    frame(input);
    const std::string longQuery = std::string(3000, 'a') + " été";
    io.AddInputCharactersUTF8(longQuery.c_str());
    frame(input);
    check(filter.query == longQuery, "filter input resizes without truncating pasted text");
    frame(input, {-100, -100}, -1, false, ImGuiKey_Escape);
    check(filter.query.empty(), "Escape clears the focused filter");
    frame(input);
    ImGui::ClearActiveID();

    // Only the focused sibling panel receives Ctrl+F / Cmd+F.
    for (bool mac : {false, true}) {
        const bool previous = io.ConfigMacOSXBehaviors;
        io.ConfigMacOSXBehaviors = mac;
        TextFilter left, right;
        bool focusRight = true;
        const auto panels = [&] {
            ImGui::BeginChild("left", {350, 120});
            panelHeader("Left", nullptr, &left);
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("right", {350, 120});
            if (focusRight) {
                ImGui::SetWindowFocus();
                focusRight = false;
            }
            panelHeader("Right", nullptr, &right);
            ImGui::EndChild();
        };
        frame(panels);
        frame(panels);
        io.AddKeyEvent(mac ? ImGuiMod_Super : ImGuiMod_Ctrl, true);
        frame(panels, {-100, -100}, -1, false, ImGuiKey_F);
        io.AddKeyEvent(mac ? ImGuiMod_Super : ImGuiMod_Ctrl, false);
        frame(panels);
        frame(panels);
        io.AddInputCharactersUTF8("only-right");
        frame(panels);
        check(left.query.empty() && right.query == "only-right",
              "find shortcut stays in the focused panel for Ctrl and Cmd");
        frame(panels, {-100, -100}, -1, false, ImGuiKey_Escape);
        frame(panels);
        ImGui::ClearActiveID();
        io.ConfigMacOSXBehaviors = previous;
    }

    // Clicking a filtered leaf invokes its stable host ID. A disabled leaf cannot execute.
    TextFilter actionFilter;
    actionFilter.query = "png";
    ImVec2 actionPosition;
    std::uint64_t request = 0;
    const auto drawMenu = [&] {
        request = filterMenu("menu", actionFilter, menu, {260, false, false});
        const auto a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        actionPosition = {(a.x + b.x) * .5f, (a.y + b.y) * .5f};
    };
    frame(drawMenu);
    frame(drawMenu);
    frame(drawMenu, actionPosition, 0, true);
    frame(drawMenu, actionPosition, 0, false);
    check(request == 0x100000001ull, "filtered menu click returns the original action ID");
    actionFilter.query = "hdr";
    frame(drawMenu);
    frame(drawMenu);
    frame(drawMenu, actionPosition, 0, true);
    frame(drawMenu, actionPosition, 0, false);
    check(request == 0, "disabled search result does not execute");

    // A real popup resets its query on open and allows direct typing into its filter.
    TextFilter popup;
    popup.query = "previous";
    bool open = true;
    const auto drawPopup = [&] {
        if (open) {
            ImGui::OpenPopup("popup");
            open = false;
        }
        if (ImGui::BeginPopup("popup")) {
            filterMenu("actions", popup, menu);
            ImGui::EndPopup();
        }
    };
    frame(drawPopup);
    frame(drawPopup);
    frame(drawPopup);
    check(popup.query.empty(), "menu query resets on popup open");
    io.AddInputCharactersUTF8("png");
    frame(drawPopup);
    check(popup.query == "png", "menu filter receives typing on open");
    ImGui::DestroyContext();
    if (!failures)
        std::puts("CyUI filter models, sections, text entry and menu actions passed");
    return failures ? 1 : 0;
}
