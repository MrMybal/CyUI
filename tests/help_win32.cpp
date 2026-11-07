#include "../examples/demo_win32_dx11/Help.h"
#include <iostream>

int main() {
    // Platform URL creation must escape path punctuation before adding the route.
    const auto uri = demo_help::url(std::filesystem::absolute(L"build/Manuel été #100%/Documentation.html"),
                                   "fr", "integration", "startup");
    if (uri.rfind(L"file:///", 0) != 0 || uri.find(L"%20") == std::wstring::npos ||
        uri.find(L"%23") == std::wstring::npos || uri.find(L"%25") == std::wstring::npos ||
        uri.substr(uri.find(L'#')) != L"#/fr/integration/startup") {
        std::wcerr << uri << L'\n'; return 1;
    }
    if (demo_help::encode("été /#?") != "%C3%A9t%C3%A9%20%2F%23%3F") return 2;
    const auto reader = demo_help::readerPath();
    if (reader.filename() != L"Documentation.html" || reader.parent_path().filename() != L"Documentation") return 3;
    return 0;
}
