---
id: localization
title: Localisation
description: Traduire les interfaces C++, Python, Web et Unreal sans changer les identifiants.
section: Guides
order: 33
sourceRevision: 1
---

La localisation appartient au Core et n’ajoute aucune dépendance optionnelle. L’anglais (`en`) est la langue par défaut et le français (`fr`) est intégré. Les contrôles des modules utilisent ce catalogue ; les galeries C++ et Python proposent un sélecteur immédiat. Les mêmes messages sont accessibles aux vues Web embarquées.

## C++ {#c}

Incluez `<cyui/Core.h>` ou `<cyui/Locale.h>`. Choisissez la langue après la création du contexte de la vue, avant de dessiner :

```cpp
cyui::setLocale("fr");                 // omit to keep English
if (cyui::primaryButton(cyui::trLabel("Save").c_str()))
    saveDocument();
cyui::text("%s", cyui::tr("Ready").c_str());
```

Utilisez `tr` pour l’affichage, `trLabel` pour les contrôles, titres de fenêtre, menus et onglets. Le libellé traduit conserve l’identité ImGui originale, y compris les suffixes `###id` et `##suffix` : un changement de langue conserve sélection, focus et édition. Ne reconstruisez pas un identifiant à partir d’une légende traduite.

L’application peut fusionner ses messages ou ajouter une langue :

```cpp
cyui::registerTranslations("fr", {{"app.export", "Exporter"}});
cyui::registerTranslations("de", {{"Save", "Speichern"}}, "Deutsch");
if (cyui::button(cyui::trLabel("app.export", "Export").c_str()))
    exportDocument();
```

`availableLocales()` retourne anglais, français, puis les langues enregistrées. `locale()` donne le code actif. Les tags sont normalisés (`fr_CA` devient `fr-ca`) ; une variante régionale non enregistrée retombe sur sa langue (`fr`). Un message manquant cherche ensuite la langue de base, le message anglais enregistré, le texte anglais fourni, puis la clé. `setLocale` retourne false pour une langue indisponible et conserve la langue courante.

La langue C++ appartient au contexte ImGui courant ; les catalogues sont partagés. Sans contexte courant, `setLocale` change les valeurs par défaut et les contextes existants : aucun autre contexte ne doit alors être en train de dessiner. Enregistrez également les catalogues hors du rendu concurrent. Détruire le contexte libère son état de langue. Conservez la chaîne retournée si son `c_str()` doit vivre après l’appel ; ne stockez pas un pointeur vers une traduction temporaire.

## Python {#python}

L’API existe dans `cyui` et, sans ImGui, dans `cyui_modules.locale` :

```python
import cyui as c
c.set_locale('fr')
if c.primary_button(c.tr_label('Save')):
    save_document()
c.register_translations('fr', {'app.export': 'Exporter'})
label = c.tr('app.export', 'Export')
```

`available_locales()` retourne des couples `(code, nom)`. `locale_scope('fr')` sélectionne temporairement une langue et restaure la précédente. Python utilise `ContextVar` : l’état appartient au contexte d’exécution, pas à un pointeur ImGui. Plusieurs vues peuvent entourer leur rendu d’un scope et des tâches asynchrones indépendantes garder leurs langues. Les catalogues restent partagés dans l’interpréteur.

## Web et TypeScript {#web-and-typescript}

Chaque `createCyUIBridge()` a son propre catalogue `bridge.i18n`. `webThemeJson()` en C++ et `theme_message()` en Python incluent `locale`. Renvoyez le thème après un changement de langue, ou un message `locale` avec `payload: {locale: 'fr'}` dans l’enveloppe versionnée habituelle.

```html
<button data-cyui-i18n="Save">Save</button>
<input data-cyui-i18n-placeholder="Search...">
```

Le bridge traduit les éléments marqués et les placeholders, puis met à jour la langue du document. Les contenus non marqués et les valeurs saisies restent inchangés. Dans un framework, appelez `bridge.i18n.tr(key, englishFallback)` au rendu. `createCyUILocale()` fonctionne sans bridge natif. Après ajout de messages ou `setLocale` direct, rappelez `apply()` ou déclenchez le rendu ; un message natif entrant appelle `apply()`.

Distribuez `cyui-i18n.js` et `cyui-locales.js` avec `cyui-web.js`. Une iframe possède sa propre langue et reçoit celle-ci par son protocole. `mountCyMD().open(name, file, locale)` utilise l’anglais par défaut.

## Unreal Editor {#unreal-editor}

Appelez `setLocale` dans `RunInContext` ou le callback de dessin du panneau. Les panneaux ont des contextes indépendants et peuvent utiliser des langues différentes. Enregistrez les messages applicatifs pendant l’initialisation de l’outil. Les menus Slate/Unreal utilisent le système de traduction Unreal ; CyUI traduit son propre contenu.

## Ce que l’application traduit {#what-applications-translate}

CyUI traduit ses contrôles, invitations par défaut et galerie. L’hôte traduit ses actions, validations et légendes de modèles. Noms/aides de propriétés, choix, catégories de commandes, invitations de champs, légendes de potentiomètres et infobulles utilisent le catalogue ; les valeurs stockées et les identifiants restent inchangés. Fournissez une clé anglaise ou une clé applicative enregistrée.

Texte libre, fichiers, assets, graphes, code source, diffs, journaux, documents et messages restent des données utilisateur. Affichez-les avec un format fixe comme `text("%s", content.c_str())`. L’hôte conserve la préférence et l’applique au démarrage ; CyUI ne la choisit pas depuis l’OS.

Les textes sont UTF-8. Chargez des polices couvrant vos langues ; celles de la démo Windows couvrent anglais et français. CyUI ne fournit pas la disposition générale droite-à-gauche, la composition complexe, les règles de pluriel ou le formatage régional des nombres/dates : l’hôte doit les prendre en charge.

```image
{"id": "localization-view", "src": "assets/inspector.webp", "compare": "assets/inspector-fr.webp", "labels": ["Anglais", "Français"], "alt": "Inspecteur de propriétés en français", "controls": "minimal"}
```
