# Developer maintenance commands

These procedures maintain CyUI itself. Human integration help lives in the CyDoc manual.


## Dependency update

`third_party/code-syntax.lock.json` records exact Lexilla, Scintilla-header and
SciTE-property commits and SHA-256 archive checksums. Sources and generated data
are vendored; only 79 selected engines (72 lexer translation units) plus Lexilla's
support library compile. Scintilla's editing UI is not included. Preserve all
three licenses and THIRD_PARTY_NOTICES.md when redistributing Code.

To regenerate, obtain the three locked trees, verify checksums, then run
`python tools/generate_code_languages.py --help` and supply their roots. Review
the generated diff and run the `cyui_code_languages` CTest. It tests all 102
fixtures, associations, shaders, custom rules, inherited engines, custom callback
validation, registry updates/removal and native editor revision/Undo behavior.




## Maintaining the built-in catalogs

Edit `locales/en.json` and `locales/fr.json`, then run:

```sh
python -B tools/generate_locales.py
python -B tools/generate_locales.py --check
```

The generator checks matching keys and printf placeholders and produces the
dependency-free C++, Python and JavaScript catalogs. Keep an English fallback for
every message, preserve stable IDs, and translate interface strings rather than
model data. CI checks that the generated catalogs are current.


## Validation

```sh
cmake -S . -B build/check -DCYUI_BUILD_DEMO=OFF -DCYUI_MODULES=All -DCYUI_BUILD_TESTS=ON
cmake --build build/check --config Release
ctest --test-dir build/check -C Release --output-on-failure
python -B tests/test_python_modules.py
node --test tests/web_bridge.test.mjs
python -B tests/module_matrix.py
```

module_matrix.py supports `--generator`, `--make-program`, `--cmake` and an
optional prebuilt host `--imgui-library` to avoid recompiling ImGui in each variant.
It builds, links and runs each module separately and checks the dependency closure.
The feature tests include real ImGui frames and gestures, UTF-8/CRLF/tab round trips,
revision cancellation, exact int64 properties, bounded concurrent logs and diff
reconstruction properties. `tests/capture_gallery.py` covers tabs 0–21 across all
five themes and both densities with the Windows demo.

GitHub Actions is configured for Windows, Linux and macOS native builds. A workflow
file is not evidence that remote jobs have run. The local build/render validation
for this revision uses Windows/MSVC; platform-specific CEF packaging still needs
validation on each target OS. See [WEB.md](WEB.md) and [LICENSING.md](LICENSING.md).

See [CODE.md](CODE.md) for the C++ syntax catalogue, plugin extension contract,
UTF-8 span rules, caching and reproducible lexer updates.
