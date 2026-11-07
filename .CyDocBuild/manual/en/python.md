---
id: python
title: Python integration
description: Install the Python UI, import models and run the module gallery.
section: Start
order: 4
revision: 1
---

## Installation {#installation}

CyUI requires Python 3.10+. Install the checkout with `python -m pip install -e .`, or install its built wheel. imgui-bundle 1.92+ is the declared drawing dependency; the historical `[ui]` extra remains compatible. Model-only imports under cyui_modules do not import the renderer, media decoders, NumPy or CEF.

```python
from cyui_modules.common import Range
from cyui_modules.diff import DiffDocument
comparison = DiffDocument()
comparison.set_texts('old\n', 'new\n', 'plain')
```

## Draw a gallery {#gallery}

Run `python examples/python_modules.py` with imgui-bundle installed. Its ModuleGallery can also be embedded in your existing application. Create one gallery per host view, call draw during the GUI frame, and keep host textures/Web backends alive. Theme and language selectors update live; use the Documentation button or F1 for contextual help.

Use `cyui.apply()` once after context creation, `cyui.set_locale('fr')` for French, and `cyui.render_notifications()` once per frame. Follow [the module edit recipes](doc:module-recipes#python-examples), [translation scopes](doc:localization#python) and [filters](doc:filtering#python).

## Native and Python differences {#limits}

Python optional families use snake_case APIs. Its CodeEditor uses ImGui multiline editing, native Undo and a live syntax preview. The C++ TextEditor multiple cursors/completion hooks and native syntax-plugin registry are not Python bindings. Python SourceSpan offsets are UTF-8 bytes; displayed Python lexer spans use character indices. The Diff renderer is independent of the native C++ shared-canvas implementation.

CEF is explicitly loaded through CEFRuntime; its library, helpers and Chromium runtime are supplied separately. [The Web guide](doc:web#python) explains thread, texture and shutdown ownership. The Python wheel includes the entire local manual as cyui_documentation package data; no Node.js is required to read it.
