---
id: module-recipes
title: API et recettes des modules
description: Données, gestes et retours d’action des vues spécialisées.
section: API
order: 21
sourceRevision: 2
---

Les en-têtes utilisent le préfixe `cyui/` ; liez leurs [cibles correspondantes](doc:modules). Gallery.cpp, onglets 4–19, contient les exemples. Les callbacks et superpositions de l’hôte suivent les règles palette/dp/style.

### Identifiants, états et éditions {#ids-state-and-edits}

`Interaction.h` définit ItemId (uint64), Point (double x/y), Range (min/max), Rect (bornes écran), ArrayView emprunté et Edit. Zéro représente l’absence d’objet dans les résultats ; utilisez des IDs stables non nuls. ItemScope place les 64 bits dans la pile ImGui. Gardez l’état par panneau, pas un geste statique partagé entre documents. snapValue arrondit symétriquement ; zoomRange conserve l’ancrage du pointeur.

Les éditeurs proposent begin/update/commit/cancel. Un commit devient une transaction Undo de l’application. Une nouvelle révision annule un geste périmé. Les actions atomiques peuvent valider directement. Ne remplacez pas le modèle par les aperçus sauf si l’hôte gère explicitement cette couche.

```cpp
cyui::CurveState state;                   // persist with the panel
std::vector<cyui::CurvePoint> points;      // sorted, finite, stable nonzero IDs
cyui::CurveOptions options;
options.revision = document.revision;
options.snap = 0.25;
auto edit = cyui::curveEditor("##automation", state, points, cyui::dp(0, 260), options);
if (edit.committed()) document.commitCurve(edit.value);
```

Les modèles n’embarquent ni pointeur moteur, ni chaîne JUCE, ni JSON obligatoire. Gardez tableaux et captures vivants durant le dessin.

### Canevas et tracés numériques {#canvas-and-numeric-plots}

| API | Usage |
| --- | --- |
| `CanvasView::world/screen/zoomAt/fit` | Coordonnées doubles et transformations ancrées |
| `CanvasScope(id,state,size,options)` / `frame()` | Fond, grille et clipping ; frame().wheel si la navigation/claimWheel capture la molette |
| `drawCanvasGrid(rect,view,step)` | Grille dans la draw list courante |
| `Axis::fraction/value` | Conversion linéaire/log inverse ; bornes logarithmiques positives |
| `plot(id,state,series,size,options)` | Lignes, marches, points, barres, axes/légende, zoom et échantillon survolé |
| `reducePlot(points,budget)` | Seaux min/max qui préservent pics et lacunes |
| `histogram(values,range,bins)` | Comptes uint64, bornes incluses, NaN/hors-plage ignorés |
| `heatmap(id,values,columns,rows,valueRange,size)` | Cellules bornées, données manquantes, ligne/colonne survolée |

Canevas : bouton milieu ou Espace+gauche pour déplacer, molette pour zoomer au pointeur. Tracé : milieu déplace, molette zoome X et Ctrl+molette ajoute Y. PlotOptions règle grille, axes, légende, réticule/navigation, maxPoints et formatX/formatY pour les unités. Les graduations et la légende restent à l’intérieur. Si skippedSeries est renseigné, réduisez/interrogez les données avant le dessin ; aucune réduction coûteuse n’est faite silencieusement à chaque frame.

```cpp
cyui::PlotState state{{{20, 20000}, true}, {{-90, 0}, false}};
cyui::PlotSeries line{1, "Spectrum", preparedPoints, cyui::PlotStyle::line, 1};
cyui::PlotOptions options;
options.formatX = [](double hz) { return formatFrequency(hz); };
auto hover = cyui::plot("##response", state, {&line, 1}, cyui::dp(0, 220), options);
```

### Courbes, graphes et timeline {#curves-graphs-and-timeline}

`evaluateCurve(points,time,fallback)` évalue des clés triées en mode linéaire, marche, lisse ou Bézier. `curveEditor` retourne des propositions de points/poignées. Double-clic ajoute/supprime ; glisser déplace point/poignée ; Ctrl étend la sélection, glisser le vide sélectionne un rectangle. Ctrl+A/C/V, Suppr et Échap sont limités au panneau focalisé. CurveOptions.evaluate permet l’interpolation de l’hôte et snapTime remplace la grille par défaut.

`arrangeNodes(nodes,links,spacing)` propose des positions déterministes par couches et tolère les cycles. `nodeEditor` retourne moved, connectFrom/connectTo et removeNodes/removeLink. Glissez cartes/ports ; Ctrl/Shift multisélectionnent, le vide crée un rectangle, Home cadre, Suppr propose une suppression, clic droit sur un lien dans le vide propose sa déconnexion. canConnect valide types et règles avant la requête. drawBody dessine le contenu de la carte sans exécuter le graphe. L’hôte alloue les nouveaux IDs et retire les liens orphelins.

Le graphe d’images de la galerie utilise drawBody et une hauteur row accrue pour les aperçus sous les ports. `GalleryResources::imagePreviews` fournit jusqu’à quatre textures maintenues par l’hôte. Assets les utilise comme vignettes et Inspector comme aperçu ; sans ressource, une indication les remplace. Les pixels générés sont des données indépendantes de la palette UI.

`timeline` retourne éditions déplacement/rognage, seek, plage et IDs supprimés. Molette zoome au pointeur, Shift+molette déplace le temps, Ctrl+molette défile les pistes. Les clips verrouillés/readOnly sont respectés. Glisser le vide sélectionne une plage, la règle demande une position. TimelineOptions fournit playhead, snap/minDuration, revision, formatTime, snapTime, drawClip. Unités et arrondis de frame/échantillon appartiennent à l’hôte.

### Contrôles audio et vues de signaux préparés {#audio-controls-and-prepared-signal-views}

Audio.h conserve knob/fader/meter et meterPosition. Le fader répartit sa plage minimumDb..maximumDb sur sa course ; une taille nulle/négative suit le dimensionnement ImGui. AudioViews propose :

| API | Entrée |
| --- | --- |
| `makePeaks(samples,columns)` | Enveloppe min/max, NaN pour les seaux entièrement manquants |
| `waveform(id,state,data,playhead,size)` | Intervalle couvert et pics préparés par canal |
| `spectrum(id,state,series,size[,options])` | Fréquence/valeur, X logarithmique (20..20000 si invalide) |
| `oscilloscope(...)` | Séries temps/amplitude préparées |
| `stereoScope(...)` | Vue carrée X/Y, plage symétrique commune autour de zéro |

Waveform propose seek/plage, zoom molette, déplacement Shift+molette ou milieu. L’hôte décode, met les enveloppes en cache et calcule FFT/DSP hors dessin, en possédant les échantillons et leur synchronisation.

### Vues d’analyse audio {#audio-analysis-displays}

Ces vues appartiennent à **AudioViews**, avec les mêmes dépendances (`Plots`,
`Canvas`, `Core`). Les contrôles Audio, Curves et les SDK audio restent optionnels.
L’onglet natif **Analyse audio** (22) et la galerie Python présentent chaque vue.

| API C++ / Python | Affichage et entrées |
| --- | --- |
| `spectrogram` | Matrice temps/fréquence/intensité, cases et vue fréquentielle linéaires ou logarithmiques, couleurs de palette, échelle, réticule et case survolée exacte |
| `loudness` | LUFS momentanés, à court terme et intégrés, LRA en LU depuis les bornes absolues en LUFS, cible configurable et historiques superposés |
| `truePeak` / `true_peak` | dBTP par canal, crêtes maintenues, dépassements, repères d’avertissement/plafond, liste défilante et demande de remise à zéro |
| `correlation` | Jauge signée -1..+1 et historique optionnel ; associez `stereoScope` / `stereo_scope` pour les tracés de phase L/R ou M/S préparés |
| `filterResponse` / `filter_response` | Courbes de gain Hz/dB superposées, axe de phase indépendant à droite en degrés, points fréquence/gain éditables à identifiants stables |
| `dynamics` | Courbes de transfert entrée/sortie en dB, référence unitaire, point de fonctionnement et barre de réduction de gain en dB positifs |

```image
{"id":"audio-analysis","src":"assets/audio-analysis-fr.webp","alt":"Galerie AudioViews : spectrogramme, EQ et phase éditables, LUFS, dynamique, crête vraie et corrélation","controls":"minimal"}
```

**Matrice du spectrogramme :** `SpectrogramData.values[frame * bins + bin]` contient
les trames et cases dans l’ordre croissant sur `time` et `frequency`.
`logarithmicBins` décrit l’espacement des cases stockées indépendamment de
`PlotState.y.logarithmic`. L’unité est le dB par défaut ;
`SpectrogramOptions.intensity/unit` configure la plage de couleurs et l’unité.
Initialisez la vue sur l’intervalle couvert, par exemple
`PlotState{{{0, 8}, false}, {{20, 20000}, true}}`.
La molette zoome le temps, Ctrl+molette aussi les fréquences, et le glissement
central déplace les deux axes. `SpectrogramResult` renvoie `valid`, `hovered`,
`clicked`, `viewChanged`, `mouse`, `frame`, `bin` et `intensity`. Dimensions et plages
incorrectes sont rejetées avant indexation ; les cases NaN restent vides.
Le rendu échantillonne le centre des cellules écran, au plus `maxCells` (65 536 par
défaut, borné à 1..262 144). Cette réduction ne préserve pas les pics : préparez une
résolution visible adaptée quand de petits transitoires doivent rester visibles.

**Mesures :** NaN (`missingAudioValue`) représente une valeur indisponible et
l’infini négatif le silence. Fournissez mesures LUFS, bornes LRA, crêtes maintenues,
dépassements dBTP et séries historiques. `LoudnessOptions` et `TruePeakOptions`
configurent cibles, plages et bouton de remise à zéro. Le bouton renvoie
`AudioMeterResult.resetRequested` sans modifier les mesures fournies.
`clippedChannels` compte les canaux actuellement au-dessus du plafond ou ayant des
dépassements enregistrés. L’hôte choisit les valeurs accumulées et historiques à
effacer. L’axe Y des historiques de sonie/corrélation reste fixé à leur échelle ;
leur vue X propose la navigation des graphiques. Donnez-lui une plage temporelle.

```cpp
cyui::ResponseState state; // persistant par panneau
std::vector<cyui::ResponsePoint> bands{{1, 120, 4}, {2, 1800, -7}};
cyui::ResponseOptions options;
options.revision = revision;
auto result = cyui::filterResponse("##eq", state, gainCurves, phaseCurves,
                                  bands, cyui::dp(0, 240), options);
if (result.points.committed()) commitBands(result.points.value); // une entrée Undo
// Pendant le glissement, évaluer les courbes depuis state.preview.
```

Les points EQ ont des positions Hz/dB et des IDs 64 bits uniques non nuls (1 024
maximum). Le résultat suit begin/update/commit/cancel : déplacement en aperçu,
validation au relâchement, annulation par Échap. Un changement de révision, de
modèle fourni, du mode lecture seule ou la perte de focus de la fenêtre annule le
geste actif. Des points invalides ou dupliqués désactivent l’édition.
Les bornes fréquence/gain limitent l’édition ; la navigation règle la vue affichée.
Les séries de phase dépassant `PlotOptions.maxPoints` sont signalées dans
`result.plot.skippedSeries`, comme celles de gain. L’hôte fournit séparément les
courbes de gain et de phase, y compris celles de l’aperçu.

Python utilise les mêmes classes avec champs snake_case : `SpectrogramData`,
`LoudnessData`, `TruePeakChannel`, `ResponsePoint`, `ResponseState`, `DynamicsData`.
Les options sont des arguments nommés. Les résultats sont des dictionnaires à clés
snake_case ; `filter_response(...)["points"]` est un `Edit` avec `.phase`, `.value`,
`.committed`. L’argument des courbes `gain` se distingue des bornes d’édition
`gains=Range(-24, 24)`. Pour `dynamics`, initialisez les deux axes en dB linéaires,
par exemple -60..0 ; `DynamicsOptions.reduction` / Python `reduction=` règle
l’échelle d’atténuation.

### Code, édition et comparaison {#code-editing-and-git-style-diff-presentation}

`CodeDocument::setText(text,language,revision)` conserve UTF-8 brut, tabulations, CRLF et présence du saut final. L’affichage développe les tabulations sur quatre colonnes et porte les spans lexicaux. Le catalogue commun propose [102 modes](doc:code), dont plain, C++, C#, Python, JavaScript, TypeScript, Rust, GLSL, HLSL, JSON, Lua, SQL, INI et Markdown. Ajoutez les résultats sémantiques/LSP comme superpositions lorsque nécessaire.

- setOverlays(SourceSpan[],revision) utilise des lignes depuis zéro et des offsets en octets UTF-8. Révisions périmées et offsets internes à un point de code sont refusés.
- tokenColor(role,background) utilise la palette avec correction vers text pour le contraste ; paintCodeLine dessine les spans préparés.
- codeViewer affiche numéros, recherche/suivant, ligne sélectionnée/focalisée, infobulles d’annotations et IDs de navigation.
- CodeEditor setText/text/setLanguage/setReadOnly/draw enveloppe le TextEditor MIT ; draw indique une modification. native() expose complétion, annotations, curseurs, wrapping et Undo/Redo : incluez TextEditor.h et gérez stockage/révisions côté hôte.
- DiffDocument setTexts(before,after,language,workBudget) prépare la comparaison bornée ; setUnified lit un patch. rows/pairs/hunks/columns/revision/approximate exposent le résultat.
- diffViewer présente mode unifié ou côte à côte, un canevas vertical et un décalage horizontal communs, gouttières fixes, navigation par blocs/recherche, différences internes et anciennes/nouvelles lignes. Un changement du saut final est représenté ; CRLF/LF sont égaux avec une indication de fins de ligne différentes. Affecter DiffState.scroll restaure la position. Appelez directement cette vue sans synchroniser des fenêtres enfants. Voir [Diff](doc:diff).
- expandTabs développe les tabulations ; displayOffset(raw,byteOffset,tabSize) convertit l’offset brut vers l’affichage.

```cpp
cyui::DiffDocument comparison;
comparison.setTexts(beforeText, afterText, cyui::CodeLanguage::cpp);
cyui::DiffState view;
auto clicked = cyui::diffViewer("##comparison", view, comparison, cyui::dp(0, 400));
if (clicked.newLine > 0) host.openNewDocumentAt(clicked.newLine);
```

Ne rappelez pas setTexts à chaque frame pour un grand document inchangé. Code et Diff ne dépendent ni de Git, ni d’un LSP/compilateur, ni de CodeEditor. Authentification, dépôt, commentaires de revue, résolution de conflits et actions appartiennent à l’hôte.

### Inspector et Assets {#inspector-and-assets}

Property accepte bool, int64, double, chaînes, choix, drapeaux readOnly/password/multiline et bornes optionnelles. Utilisez integerMinimum/integerMaximum sans conversion par double. inspector(id,state,properties,revision,validator) retourne des PropertyEdit. Le validateur retourne une erreur ; les brouillons invalides restent visibles sans commit.

assetBrowser utilise AssetItem{id,parent,label,type,thumbnail,folder,favorite}. AssetState conserve recherche, dossier, IDs sélectionnés et grille/liste. La vue retourne open/favorite/rename/remove sans opération de fichier. Le payload CYUI_ASSET porte l’ID stable pour le glisser-déposer de l’hôte. Parents, vignettes, recherche, scan et chargement relèvent de l’application.

### Images, viewport et gizmo 3D optionnel {#images-viewport-and-optional-3d-gizmo}

| API | Usage |
| --- | --- |
| `imageViewport(id,state,ImageData,size,ImageTool)` | Texture/UV, damier, cadrage/zoom/déplacement, pixel et proposition de recadrage |
| `sampleGradient(stops,position,linearLight)` | Points non triés, interpolation sRGB ou lumière linéaire |
| `gradientEditor(id,state,stops,size,linearLight,revision)` | Sélection, déplacement, ajout/suppression et éditions |
| `cameraPosition/cameraViewMatrix(OrbitCamera)` | Axe vertical arbitraire, matrice colonne, repère droit |
| `viewport(id,state,ImageData,size,navigation)` | Texture conservant le ratio, UV et caméra |
| `transformGizmo(id,state,Transform2D,options)` | Déplacement/échelle/rotation 2D, projection/déprojection |
| `gizmo3D(id,state,matrix,view,projection,rect,options)` | Adaptateur ImGuizmo optionnel, matrices colonne |

Image : molette zoome, milieu déplace, double-clic ajuste ; recadrage retourne un rectangle et Échap restaure la sélection. Dégradé : double-clic ajoute un point interpolé, glisser déplace, clic droit retire si plus de deux points restent ; l’hôte édite leurs couleurs. Durée de vie des textures, gestion colorimétrique et transformations de document restent à l’hôte.

Les poignées du gizmo sont des éléments ImGui : elles prennent survol/clic avant le canevas sous-jacent. Un dessin hôte au-dessus du canevas appelle markCanvasOverlay chaque frame. Viewport : Alt+gauche orbite, milieu déplace, molette règle la distance. CyUI ne rend pas de scène 3D : l’hôte fournit sa texture. La rotation Transform2D est en radians ; Ctrl aligne à 15 degrés. Gizmo3DOptions règle opération/mode, snap, projection orthographique, révision et activation. Liez CyUI::ImGuizmo uniquement avec une cible hôte compatible.

### Commandes, journaux et documents {#commands-logs-and-documents}

- rankCommands correspond à tous les mots et retourne des indices classés stables. commandPalette affiche une modale navigable au clavier et retourne un ID. Les commandes désactivées ne s’exécutent pas ; l’hôte associe les actions.
- LogBuffer(capacity,maxBytes) synchronise append/clear/snapshot. logView filtre/suit des lignes bornées en cache et retourne l’ID sélectionné ; LogState.levels masque debug/info/warning/error. diagnosticList affiche source/ligne/colonne/message et retourne un ID ; l’hôte ouvre la source.
- Document::setMarkdown prépare titres, paragraphes, citations, listes, règles, code clôturé, liens, emphase/code inline et tables à barres. documentView retourne liens/copies sans les ouvrir.
- transcript ignore les messages hors écran déjà mesurés, met les Documents en cache par ID, suit le contenu ajouté/streamé lorsque la lecture est en bas et retourne copie/liens. Un message contient id/role/content/time. Pour un rendu CyMD complet, utilisez Web.

Le sous-ensemble natif n’exécute ni HTML brut ni script. Il utilise le Core et les couleurs de Code ; l’emphase est un sous-ensemble visuel, pas un moteur CommonMark complet.

### Web / CEF optionnel {#optional-web-cef}

webPanel, WebBackend/WebPlacement/WebInput, WebPolicy/webOrigin, webThemeJson et webEventJson composent l’API indépendante du moteur. createWebCEF, webCEFApp et webCEFBrowser apportent CEF optionnel. Le navigateur dessine dans une texture de l’hôte et partage les rôles de thème via CSS. Voir [le cycle complet, Python/TypeScript, origines et distribution](doc:web). Le runtime validé localement est Windows ; les builds/déploiements Linux et macOS restent à valider.

Pompez CefDoMessageLoopWork/runtime.pump avant NewFrame, jamais pendant le dessin. upload/release y sont exécutés ; libérez la dernière texture après fermeture et après les frames qui l’utilisaient. Les événements external sont des demandes de page : vérifiez scheme/origine, userGesture, mainFrame, sourceOrigin et demandez confirmation avant une ouverture shell. Les popups de sous-frames sans geste sont ignorés. dropped porte le nombre de rejets par les limites de file ou politique. focus est pris une fois au passage false→true ; les touches transmises appartiennent au panneau focalisé. Les appels backend s’effectuent sur le thread UI CEF ; les autres sont ignorés. Bridge/helper/démo ne sont pas sandboxés sous Windows.

### Exemples Python {#python-examples}

```python
from cyui_modules.curves import CurvePoint, CurveState, curve_editor
from cyui_modules.common import Range

state = CurveState()
state.view.x.view = Range(0, 10)
state.view.y.view = Range(0, 1)
points = [CurvePoint(1, 0, .2), CurvePoint(2, 10, .8)]
# Each ImGui frame:
edit = curve_editor('automation', state, points, (0, 260), revision=revision)
if edit.committed:
    points = edit.value
```

Les noms Python sont snake_case dans les paquets [listés ici](doc:modules#python). SourceSpan utilise des octets UTF-8 bruts, tandis que les spans d’affichage du lexer Python sont des indices de caractères : ne les confondez pas. L’éditeur Python possède Undo multiligne ImGui et aperçu syntaxique, sans callbacks avancés TextEditor C++. CEFRuntime charge le navigateur explicitement. Les imports de modèles ne chargent ni imgui-bundle, ni NumPy, ni moteur média/navigateur.

```image
{"id": "module-recipes-view", "src": "assets/image-graph.webp", "alt": "Cartes de nœuds avec aperçus générés par l’hôte", "controls": "minimal"}
```
