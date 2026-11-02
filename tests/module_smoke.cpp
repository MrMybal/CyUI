#include <cyui/Core.h>

#if defined(CYUI_TEST_COMPAT)
#include <cyui/CyUI.h>
#include <cyui/Widgets.h>
#elif defined(CYUI_TEST_AUDIO)
#include <cyui/modules/Audio.h>
#elif defined(CYUI_TEST_GALLERY)
#include <cyui/Gallery.h>
#endif

#include <cmath>
#include <cstdio>

namespace {
// Taking the address forces the linker to resolve the owning object file.
bool (*volatile coreButton)(const char*, cyui::Variant, ImVec2) = cyui::button;
#if defined(CYUI_TEST_AUDIO) || (defined(CYUI_TEST_COMPAT) && defined(CYUI_HAS_AUDIO))
bool (*volatile audioKnob)(const char*, float*, float, float, float, const cyui::KnobStyle&) = cyui::knob;
#endif
#if defined(CYUI_TEST_GALLERY) || (defined(CYUI_TEST_COMPAT) && defined(CYUI_HAS_GALLERY))
void (*volatile gallery)(bool*, int) = cyui::showGallery;
#endif
}

int main()
{
    ImGui::CreateContext();
    cyui::Config config;
    config.dpiScale = 1.5f;
    cyui::apply(config);
    bool ok = coreButton != nullptr && std::abs(cyui::dp(10.0f) - 15.0f) < 0.01f;
    cyui::setTheme("navy");
    ok = ok && cyui::currentThemeKey() == "navy";
#if defined(CYUI_TEST_AUDIO) || (defined(CYUI_TEST_COMPAT) && defined(CYUI_HAS_AUDIO))
    ok = ok && audioKnob != nullptr && cyui::meterPosition(-70.0f) == 0.0f
        && cyui::meterPosition(6.0f) == 1.0f
        && cyui::meterPosition(-12.0f) < cyui::meterPosition(0.0f);
#endif
#if defined(CYUI_TEST_GALLERY) || (defined(CYUI_TEST_COMPAT) && defined(CYUI_HAS_GALLERY))
    ok = ok && gallery != nullptr;
#endif
    ImGui::DestroyContext();
    if (!ok)
        std::fputs("CyUI module integration failed\n", stderr);
    return ok ? 0 : 1;
}
