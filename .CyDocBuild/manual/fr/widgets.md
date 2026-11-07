---
id: widgets
title: Référence des widgets
description: API du thème, des contrôles, conteneurs et barres de disposition.
section: API
order: 20
sourceRevision: 1
---

Les fonctions appartiennent à `namespace cyui`. Les en-têtes publics portent leurs commentaires détaillés ; l’index API répertorie les dépendances d’inclusion sans publier les corps d’implémentation.

**Unités** : les paramètres `size` et `width` des widgets sont en pixels écran : `button("OK", dp(80, 0))`. Zéro signifie automatique, une largeur négative remplit la ligne moins sa valeur absolue. Les hauteurs de `beginCard`, largeurs d’étiquette `beginProperties`, largeurs `beginSidebar/beginModal`, `space`, espacement de `divider`, rayon de `spinner` et diamètre `KnobStyle` sont des unités de conception mises à l’échelle à l’intérieur. La hauteur de `beginTopBar` est en pixels.

Sous `ScopedDisabled`, les composants dessinés par CyUI s’atténuent et cessent de réagir. Les changements de valeur marquent l’élément édité (`IsItemEdited`). Clics et glissements signalent `IsItemDeactivatedAfterEdit` au relâchement, permettant un Undo par geste. Molette et saisies ponctuelles ne marquent que `IsItemEdited`. `markEdited(id)` permet la même chose pour un contrôle personnalisé.

### Thème (`Theme.h`) {#theme-themeh}

| API | Rôle |
| --- | --- |
| `apply(Config)`, `applyDpi(s)`, `dpi()`, `loadFonts(FontConfig)` | Initialisation, échelle courante, chargement des polices |
| `palette()`, `setPalette(p)`, `setTheme(key)`, `builtInThemes()`, `currentThemeKey()` | Palette et thèmes intégrés |
| `paletteFields()` | Clé, libellé, groupe et pointeur de membre de chaque rôle |
| `setDensity(d)`, `density()`, `metrics()` | Taille de police, marges, arrondis, cardPadding et toolbarHeight |
| `uiFont()`, `boldFont()`, `monoFont()`, `iconsAvailable()` | Polices, nulles tant que l’atlas n’est pas chargé |
| `ScopedFont(nullptr, factor)` | Conserver la police courante avec une échelle |
| `rgb(0xRRGGBB, a)`, `alpha`, `mix`, `brighter`, `darker`, `vec`, `categorical(i)` | Couleurs ; 12 teintes catégorielles adaptées au fond |
| `dp(v)`, `dp(x,y)` | Dimensions adaptées aux DPI |
| `savePalette`, `loadPalette`, `paletteToText`, `paletteFromText` | Fichiers `.cytheme`, `key = #RRGGBB[AA]`, et ancien format JSON CyDAW |

### Icônes (`Icons.h`) {#icons-iconsh}

104 chaînes UTF-8 sont disponibles sous `cyui::icon` : play, pause, stop, record, add, remove, trash, settings, search, folder, save, undo, redo, edit, close, check, chevronDown, more, refresh, zoomIn, lock, filter, loop, library, star, copy, paste, info, warning, home, globe, people, mail, calendar, clock, cloud, upload, download, code, photo, document et autres. Elles utilisent Segoe MDL2 Assets sous Windows et peuvent être combinées à un libellé.

Sans glyphe disponible, `iconButton` montre la première lettre de son infobulle, `iconTextButton` le texte seul, `sidebarItem/emptyState` omettent l’icône et `helpMarker` affiche `(?)`. Une police d’icônes fusionnée par l’hôte fonctionne aussi. Vérifiez `iconsAvailable()` avant vos propres libellés contenant des glyphes.

### Texte (`CoreWidgets.h`) {#text-corewidgetsh}

`heading(t,level)`, `sectionTitle(t)`, `overline(T)` hiérarchisent le texte. `textDim`, `textFaint`, `textColoured`, `textMono` utilisent un format printf. `kbd`, `helpMarker`, `tooltip` expliquent les actions. `clippedText` et `centredText` dessinent du texte dans des bornes. Utilisez un format fixe pour les données utilisateur.

### Boutons {#buttons}

`toneColour(Tone)` retourne la couleur du ton (`neutral` donne `textDim`). `button(label,Variant,size)` accepte secondary, primary, ghost, outline, danger et success. Les raccourcis sont `primaryButton`, `dangerButton`, `ghostButton`. Autres contrôles :

- `iconTextButton(icon,label,variant)` ; `iconButton(icon,tooltip,active,activeColour,size)`.
- `toggleButton(label,&b,onColour,size,tooltip)` pour de petits boutons M/S/R ; `toggleSwitch(label,&b)` pour un réglage.
- `segmented(id,labels,n,&current,width)` ; largeur -1 pour remplir la ligne.
- `linkText(label)` retourne l’action ; l’hôte ouvre sa destination.

### Valeurs {#values}

Audio fournit `knob(id,&v,min,max,default,KnobStyle{diameter,colour,label,format,unit})`. `flags` accepte knobBipolar, knobLogarithmic, knobInteger et knobNoLabel ; `steps` et `modulation` sont aussi disponibles. `fader(id,&dB,minDb,maxDb,size,colour)` et `meter(id,size,peakL,peakR,rmsL,rmsR,vertical)` complètent l’audio.

`dragValue(id,&v,speed,min,max,fmt,width)` utilise une vitesse par pixel ; une fois focalisé, un cran de molette ajoute 10 × speed, sinon la molette défile le panneau. `min >= max` signifie sans bornes. `sliderBar(id,&v,min,max,fmt,width,colour)` répond au clic/glissement et à des crans de 1 % de la plage.

Ces contrôles partagent le réglage fin Ctrl ×0,15 et rapide Shift ×4. La molette capturée ne défile pas le panneau. Le double-clic ouvre la saisie pour dragValue/sliderBar et réinitialise un knob ; cliquer la valeur du knob ouvre la saisie. `valueEntry` fournit le champ et `valueEntryOpen()` permet de suspendre les raccourcis. `itemWheel()` revendique la molette pour un contrôle personnalisé ; `takeWheel`, `wheelTaken`, `valueSpeed` complètent cette API.

### Champs {#fields}

`searchField(id,buf,size,hint,width)`, `textField(id,buf,size,hint,width)` et `combo(id,&i,items,n,width)` présentent recherche, saisie et choix.

### Filtrage rapide (`Filter.h`, Core) {#quick-filtering-filterh-included-in-core}

Gardez un `TextFilter` par vue. `filterBox` dessine la requête ; `panelHeader(title,subtitle,&state)` l’intègre au titre. `state.matches(label,keywords)` examine tous les mots, les traductions et les accents latins en ignorant les IDs cachés. `beginFilteredSection(label,state,hasMatchingChildren,defaultOpen)` ouvre provisoirement une section ; appelez `endSection` si elle est ouverte. `filterMenu` reçoit l’arbre FilterMenuItem dans un menu/popup déjà ouvert et retourne un ID d’action sans l’exécuter.

`InspectorOptions{&state,sectionTitle}` et `propertyMatches` filtrent les propriétés. Fournissez le modèle complet pour annuler les brouillons masqués ; annulez vous-même les aperçus d’un panneau entièrement omis. Ces API n’interceptent pas les appels ImGui bruts. Voir [les exemples complets](doc:filtering).

### Conteneurs {#containers}

- `beginCard(id,title,height,tone)` / `endCard()` : toujours appeler endCard.
- `beginSection(label,defaultOpen)` / `endSection()` : fermer uniquement si l’ouverture retourne true.
- `beginProperties(id,labelWidth)` / `property(label,help)` / `endProperties()` : fermer uniquement si ouvert. property prépare la largeur du contrôle suivant.
- `toolbarSeparator`, `divider(spacing)` et `space(h)` règlent les espacements.

### État et notifications {#status}

`badge(t,Tone)`, `statusDot(c,tip)`, `progressBar(f,size,overlay,Tone)` (indéterminée si f<0), `spinner(id,r,c)`, `emptyState(title,msg,icon)`, `callout(Tone,text)` et `colourChip(id,c,size)` couvrent les états.

`notify(text,Tone,seconds)` cible le contexte courant ; sans contexte courant, il n’est accepté que si un seul contexte est connu, sinon il est abandonné (assertion en debug). `notify(context,text,Tone,seconds)` poste vers un contexte précis depuis tout thread. `renderNotifications()` est appelé une fois par frame ; six notifications au plus sont visibles.

### Disposition (`Layout.h`) {#layout-layouth}

Dessinez dans l’ordre : menu, barre supérieure, barre d’état, barre latérale, espace de docking.

- `beginTopBar(id,h)/endTopBar`, `beginStatusBar(id)/endStatusBar` avec `statusText(Tone,fmt,...)`.
- `beginSidebar(id,w)/endSidebar`, `sidebarHeader(title,badge)`, `sidebarGroup(LABEL)`, `sidebarItem(icon,label,active,badge)`.
- `dockSpace(id,flags)` avec la branche docking ; `panelHeader(title,subtitle[, &filter])`.
- `beginToolbar(id)/endToolbar` ; `alignRight(width)` et `buttonWidth(label)`.
- `beginModal(title,&open,width)/endModal` : fermer uniquement si ouverte. `modalButtons(confirm,cancel,danger)` retourne 1 ou 2. Entrée confirme, Échap annule ; chaque appel ferme sa propre modale, même imbriquée.

Barres, sidebar et toolbar doivent toujours être fermées, quel que soit le résultat d’ouverture. Les popups et infobulles ouverts depuis une barre conservent le style.

### Galerie (`Gallery.h`) {#gallery-galleryh}

`showGallery(&open,selectTab)` présente widgets, choix de thème et densité. `selectTab` sélectionne une fois une page : 0 texte, 1 boutons, 2 contrôles, 3 conteneurs, 4–19 modules, 20 disposition, 21 filtres. La surcharge avec `GalleryResources` accepte texture/width/height pour Images/Viewport, web pour un backend navigateur actif et previewCommands. `showThemeEditor(&open)` édite la palette et enregistre/charge `.cytheme`.

Le callback optionnel `GalleryResources::openDocumentation(page,section)` transmet
l’action Aide de l’exemple de filtrage au lecteur local de l’hôte, à
`filtering/searchable-menus`. Gardez son propriétaire vivant pendant le dessin.
Gallery n’ouvre pas de navigateur elle-même.
