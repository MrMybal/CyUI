---
id: python
title: Intégration Python
description: Installer l’interface Python, importer les modèles et lancer la galerie.
section: Découvrir
order: 4
sourceRevision: 1
---

## Installation {#installation}

CyUI demande Python 3.10+. Installez le checkout avec `python -m pip install -e .` ou sa wheel construite. imgui-bundle 1.92+ est la dépendance de dessin déclarée ; l’ancien extra `[ui]` reste compatible. Les imports de modèles dans cyui_modules ne chargent ni moteur de rendu, ni décodeur, ni NumPy, ni CEF.

```python
from cyui_modules.common import Range
from cyui_modules.diff import DiffDocument
comparison = DiffDocument()
comparison.set_texts('old\n', 'new\n', 'plain')
```

## Dessiner une galerie {#gallery}

Lancez `python examples/python_modules.py` avec imgui-bundle installé. ModuleGallery peut aussi s’incorporer dans votre application. Créez une galerie par vue hôte, appelez draw pendant la frame GUI et conservez textures/backends Web tant qu’ils sont utilisés. Thème et langue changent immédiatement ; Documentation ou F1 ouvre l’aide contextuelle.

Appelez `cyui.apply()` après création du contexte, `cyui.set_locale('fr')` pour le français et `cyui.render_notifications()` une fois par frame. Consultez [les recettes](doc:module-recipes#python-examples), [les scopes de traduction](doc:localization#python) et [les filtres](doc:filtering#python).

## Différences avec le natif {#limits}

Les familles optionnelles utilisent snake_case. CodeEditor Python est une saisie multiligne ImGui avec Undo natif et aperçu syntaxique. Multicurseurs/complétion TextEditor C++ et registre natif de plugins syntaxiques ne sont pas des liaisons Python. SourceSpan compte les octets UTF-8 ; les spans d’affichage du lexer Python comptent les caractères. Le rendu Diff Python est indépendant du canevas commun natif.

CEFRuntime charge explicitement CEF ; bibliothèque, helpers et runtime Chromium sont fournis séparément. [Le guide Web](doc:web#python) détaille threads, textures et fermeture. La wheel Python contient le manuel local entier comme ressources du paquet cyui_documentation ; Node.js n’est pas nécessaire à sa lecture.
