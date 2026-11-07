---
id: help
title: Aide locale et liens contextuels
description: Ouvrir une page précise sans serveur ni dépendance navigateur dans Core.
section: Intégrations
order: 43
sourceRevision: 1
---

## Ouvrir le manuel {#open}

Le dossier Documentation doit rester intact à côté de cyui_demo.exe. Le menu Aide ouvre le manuel dans la langue courante. F1 fonctionne aussi dans la vue galerie seule. L’application utilise son chemin d’exécutable, pas le répertoire de travail. Si le manuel manque, une erreur lisible indique le fichier attendu.

L’hôte garde le lancement du navigateur : Core n’ajoute aucun moteur Web. Les outils Unreal peuvent copier l’export et ouvrir son URI avec le mécanisme de leur plugin ; les raccourcis Unreal existants ne sont pas détournés globalement.

## Contrat d’adresse {#routes}

Les IDs ne dépendent pas des titres traduits. Utilisez une URI file encodée par l’API de plateforme (Path.as_uri en Python, UrlCreateFromPathW dans la démo Windows), puis ajoutez les composants de route encodés :

```text
Documentation.html#/en/integration/startup
Documentation.html#/fr/filtering/searchable-menus
Documentation.html#/fr/overview?media=workspace&zoom=1.5
```

## Helper Python {#python}

Le helper n’importe pas ImGui et ne lance rien à l’import. Il utilise les ressources du paquet pour fonctionner depuis une wheel installée, ou un chemin de manuel fourni par l’hôte. Le langage se replie sur en si indisponible ; les liens avec page/section inexistantes lèvent ValueError.

```python
from cyui_modules.help import documentation_url, open_documentation
url = documentation_url('integration', 'startup', locale='fr')
open_documentation('filtering', 'searchable-menus', locale='fr')
```

## Compatibilité des liens {#legacy-links}

Les anciens fichiers Markdown de docs renvoient vers les pages correspondantes du lecteur. guide.html et fr/guide.html convertissent leurs anciens dix chapitres en routes CyDoc, avec un lien utilisable si JavaScript est désactivé. Les alias de pages start/editors/data/media/platforms restent résolus par CyDoc. Les liens externes ne sont pas requis pour lire le manuel.
