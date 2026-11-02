#include "cyui/adapters/WebCEF.h"
#if defined(OS_WIN)
#include <windows.h>
#endif
#if defined(OS_MAC)
#include "include/wrapper/cef_library_loader.h"
#endif
int main(int argc, char **argv) {
#if defined(OS_MAC)
    CefScopedLibraryLoader loader;
    if (!loader.LoadInHelper())
        return 1;
#endif
#if defined(OS_WIN)
    CefMainArgs args(GetModuleHandleW(nullptr));
#else
    CefMainArgs args(argc, argv);
#endif
    const auto result = CefExecuteProcess(args, cyui::webCEFApp(), nullptr);
    return result >= 0 ? result : 2;
}
