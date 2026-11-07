# CyUI documentation workspace

The human manual lives in `manual/en/` and `manual/fr/`. English is the default;
French is mandatory. Keep page IDs, explicit heading anchors and media IDs equal
between translations. Increment `revision` when the English meaning changes, and
update the French `sourceRevision` after translating it. The CyDoc CLI rejects
missing, empty and explicitly outdated required translations.

## Regenerate

From the CyUI repository root, using Node.js 22+ and Python 3.10+:

```sh
npm ci --prefix .CyDocBuild --omit=optional
python .CyDocBuild/build.py
python .CyDocBuild/build.py --check
python -B tests/test_documentation.py
python -B tests/test_showcase.py
```

The initial dependency installation uses npm. Once installed, generation is local.
Generation stages the authored Markdown and public headers with LF line endings
inside the private cache. This keeps derived revisions and export bytes identical
for Windows CRLF and Unix LF input, without changing the author's source files.
The exported reader has no Node, Python, server, CDN or network dependency. Open
`Documentation/Documentation.html` directly from disk. All its resources must
remain together. The same export is staged under `site/Documentation/` and in
the `cyui_documentation` Python package. Do not edit generated exports.

The Windows batch build regenerates when this toolchain is installed and always
copies the export beside the executable. CMake demo builds copy it too; select
`CYUI_REGENERATE_DOCUMENTATION=ON` to regenerate during that build. Library-only
consumers acquire no documentation tool dependency. Demo ZIPs and both Unreal
packaging layouts include the complete reader. Python wheels include it as data.

## Selected readers

Only **image** and **gallery** are selected: they document real widget/module
captures, themes and visual comparisons. Every gallery child is an image. There
is no video/GIF, 3D model or interactive documentation graph to justify another
module. Node-editor screenshots need no graph runtime. Three.js is not installed
or shipped. Screenshots and the application mark come from the existing showcase;
`build.py` copies them into the disposable assets workspace before compiling.

## Public API and translation tracking

CyDoc scans only `include/cyui`, the distributable public headers. Explicit EN/FR
annotations provide symbol contracts and examples. Its lexical `apiIndex` adds
include/import references, without full source listings or implementation bodies.
It is not a C++ parser; it does not infer declarations, overloads or compiler types.
Python usage is authored separately; no invented binding documentation is emitted.

CyDoc 0.1 does not support `sourceRevision` tags in extracted comments and lists
generated index text as untracked. `api-translations.json` additionally locks the
reviewed English/French annotation pairs. A changed annotation fails generation
until both languages have been reviewed and the lock deliberately refreshed:

```sh
node .CyDocBuild/check-api-translations.mjs --record
```

## Pinned CyDoc distribution

`vendor/cydoc-0.1.0.tgz` was packed from the MIT CyDoc repository revision
`e038c474577e1f305df3eba421f2b9c03b0f1d8d` after its normal `npm ci` / `npm run build`.
It is installed as a local tarball because this version is not published to npm.
Its integrity and generation dependencies are locked by `package-lock.json`.
Keep `vendor/CyDoc-LICENSE.txt`. Updating CyDoc requires reading its actual AI
integration/API guides again and rebuilding/verifying the manual.

The stock CLI generates the real CyDoc reader and offline classic-script modules.
`compile-reader.mjs` bundles our small bootstrap against the public `cydoc/viewer`
API. Its local providers follow CyDoc's generated providers, adding content hashes
to lazy script URLs and choosing CyUI's dark theme initially while retaining the
reader's saved theme. It keeps the genuine CyDoc UI, routes, keyboard behavior,
search and in-window media enlargement. `reader.css` changes application palette
roles only. No generated manifest, data script or reader bundle is patched by hand.

The build compares or replaces only inventoried generated files; unrelated files
in a distribution tree are preserved. Working records belong in ignored
`internal/`; dependencies, assets cache and build output are not distributed.
AI guidance stays in `docs/AI_GUIDE.md` and the generated `Documentation/DocAI.md`
sidecar, outside reader navigation and search.
