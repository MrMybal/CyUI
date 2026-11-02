# Third-party notices

CyUI's own sources, documentation, Python modules, browser bridge and adapters
are distributed under the root [MIT license](LICENSE). Dependencies retain their
own copyright and licenses. Selecting a module does not relicense its dependencies.

| Component | License | Included / optional | Notice |
| --- | --- | --- | --- |
| Dear ImGui | MIT | Demo copy; hosts may supply their own target | [third_party/imgui/LICENSE.txt](third_party/imgui/LICENSE.txt) |
| ImGuiColorTextEdit, Johan A. Goossens | MIT | Bundled only for the optional C++ CodeEditor target | [third_party/texteditor/LICENSE](third_party/texteditor/LICENSE) |
| ImGuizmo | MIT upstream | Optional host-supplied target | Preserve the license of the exact host dependency |
| Chromium Embedded Framework | BSD-style, three conditions | Optional host-supplied SDK/runtime; no CEF binaries in this repository | Preserve the selected SDK's LICENSE.txt and Chromium CREDITS.html, plus its component notices |
| imgui-bundle | MIT package, with separately licensed bundled components | Optional external Python UI dependency | Preserve notices in the installed package/distribution |

The TextEditor source is a pinned upstream snapshot. File hashes, upstream URL
and the serialization patch are recorded in
[VERSION.json](third_party/texteditor/VERSION.json). The patch preserves an absent
final newline; the original copyright and MIT license remain intact.

CEF is distributed separately for each OS/architecture. Its Chromium components
have additional notices and licenses; the CEF BSD license alone is not the complete
notice set for an application shipping the runtime. Preserve the distribution's
license/credits files, and review the exact codec/media build you choose to ship.
No CEF runtime is downloaded automatically by CyUI's CMake configuration.

System fonts (for example Segoe UI on Windows) are loaded from the operating
system or host-provided font paths. CyUI does not distribute those font files.

The Cy* applications surveyed for common UI requirements are not incorporated
dependencies. Their code, assets and web builds retain their separate licenses.
