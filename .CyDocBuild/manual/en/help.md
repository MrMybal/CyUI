---
id: help
title: Local and contextual help
description: Open a precise page without a server or browser dependency in Core.
section: Integration
order: 43
revision: 1
---

## Open the manual {#open}

Keep the complete Documentation folder next to cyui_demo.exe. The Help menu opens it in the current language. F1 also works in the gallery-only view. The application resolves from its executable, not the working directory. A missing manual produces a readable error with the expected file.

The host owns browser launching: Core adds no web engine. Unreal tools can copy the export and launch its URI through their plugin’s mechanism; existing Unreal shortcuts are not globally intercepted.

## Address contract {#routes}

IDs do not depend on translated headings. Use a file URI encoded by the platform path API (Path.as_uri in Python, UrlCreateFromPathW in the Windows demo), then append encoded route components:

```text
Documentation.html#/en/integration/startup
Documentation.html#/fr/filtering/searchable-menus
Documentation.html#/fr/overview?media=workspace&zoom=1.5
```

## Python helper {#python}

The helper imports no ImGui and launches nothing on import. It resolves packaged resources from an installed wheel, or accepts a host-supplied manual path. Unknown languages fall back to en; missing page/section targets raise ValueError.

```python
from cyui_modules.help import documentation_url, open_documentation
url = documentation_url('integration', 'startup', locale='fr')
open_documentation('filtering', 'searchable-menus', locale='fr')
```

## Legacy links {#legacy-links}

Former docs Markdown files link to their matching reader pages. guide.html and fr/guide.html convert their ten former chapter fragments into CyDoc routes, with an ordinary link when JavaScript is disabled. CyDoc preserves start/editors/data/media/platforms page aliases. External links are optional reading references.
