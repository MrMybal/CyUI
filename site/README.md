# CyUI static showcase

This directory is a self-contained website for GitHub Pages. English is the
default (`index.html`); the complete French pages are under `fr/`. Both languages
include a practical HTML integration guide (`guide.html` / `fr/guide.html`).
There was no CyDoc reader in this repository to replace. Advanced API references
remain in the existing English source documentation and are identified as such.

## Preview locally

Open `index.html` directly, or serve this directory from the repository root:

```sh
python -m http.server 8000 --bind 127.0.0.1 --directory site
```

Then visit `http://127.0.0.1:8000/`. The HTML, styles, scripts, guide and screenshots
work offline; GitHub source references and release downloads require a connection
and access to the repository. The site makes no external font, script or analytics
requests. JavaScript adds image enlargement, theme previews, the mobile menu and
module filtering; content and guide links also remain accessible without it.

## Update the pages

Edit `tools/build_showcase.py` for bilingual content and document structure.
Edit `site/assets/site.css` and `site/assets/site.js` for presentation and behavior.
Then regenerate the four HTML pages:

```sh
python tools/build_showcase.py
python tools/build_showcase.py --check
python tests/test_showcase.py
```

The generator puts the current CSS and JavaScript SHA-256 prefixes into their
URLs (`?v=...`) on every page. Regeneration is required after those files change;
the check fails on stale generated HTML or resource versions. Query links such as
`?lang=fr` redirect to the corresponding static language page, retaining its
chapter. The language selector also retains the current chapter.

## Refresh the real screenshots

On Windows, build the current gallery and capture its synthetic fixtures:

```bat
build_demo.bat
python tools/capture_showcase.py
python tools/build_showcase.py
```

`capture_showcase.py` requires Pillow. It writes lossless, metadata-free WebP
images, their dimensions and SHA-256 hashes, the gallery tab/theme/language and
the captured source revision to `assets/screenshots/manifest.json`. No other
applications or their assets are used. The main image retains its full aspect
ratio and is bounded by the available first-screen height on desktop.

## GitHub Pages readiness

Publish **the contents of `site/`** as the Pages artifact when deployment is
requested. Keep `fr/`, `assets/` and `.nojekyll` together. All resource/navigation
paths are relative, so the site works at an origin root or a project subpath
such as `/CyUI/`. There are no SPA rewrites or server-side requirements.

The `Publish CyUI showcase` workflow validates the generated pages and deploys
this directory on relevant pushes to `main`, or when started manually from
`main`. GitHub Pages must use **GitHub Actions** as its publishing source.
Only the deployment job receives Pages/OIDC write permissions. Local preview and
a local commit do not publish the site. Existing GitHub source and download links
keep their current permissions.

The copy distinguishes the portable Windows x64 release from Linux/macOS library
source targets, and current-source captures from the versioned 1.2.0 demo. CEF and
Unreal Engine are optional external runtimes/integrations. The repository has no
declared alpha/beta label, so the site states active development instead.
