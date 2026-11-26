# Notes de version de CyUI

[English](VERSION.md) · [README](README.fr.md) · [Publications](https://github.com/MrMybal/CyUI/releases)

## Versions et statut

| Composant | Version | Disponibilité |
| --- | --- | --- |
| Bibliothèque C++ et paquet Python | 1.3.0 | Version des sources C++ et du paquet Python construit depuis ce dépôt. |
| Démo autonome de la galerie | 1.2.0 | Build portable Windows x64 / DirectX 11 publié. |
| Lecteur CyDoc embarqué | 0.1.0 | Dépendance de documentation des builds actuels, avec les modules image et gallery. |

**CyUI est en développement actif.** Le projet ne déclare pas de statut produit
Alpha, Bêta ou Stable. Le numéro de version de la bibliothèque et le marquage de
la publication de la démo n’établissent ni une API stable ni une validation des plateformes.

## Bibliothèque 1.3.0 — changements depuis la 1.2.0

- **Code et Diff.** Le catalogue partagé compte 104 modes, avec la prise en charge native de Verse, de Zig (`.zig`) et de ZON (`.zon`). Lecteur, éditeur et Diff utilisent les mêmes règles de coloration. Les vues gagnent la police JetBrains Mono embarquée, le zoom et de nouvelles couleurs. Le nouvel éditeur paginé reçoit les portions de fichier fournies par l’application. Les deux colonnes d’un diff défilent désormais ensemble.
- **Audio.** Les nouvelles vues affichent sonie LUFS/LRA, crête vraie, corrélation, réponse d’égalisation, spectrogrammes défilants et tracés spatiaux à persistance. `drawCurveFill` remplit une courbe et `envelope` affiche des enveloppes ADSR, AHDSR ou personnalisées. Couleurs, marges, cadre et positions de phase sont configurables. Une poignée peut ajuster simultanément le déclin et le sustain en un geste que l’hôte peut annuler.
- **Inspecteurs et graphes.** Les modules Hierarchy, DataView et FilePicker ajoutent des vues réutilisables. L’inspecteur accepte des champs personnalisés, des tableaux imbriqués virtualisés, des dictionnaires et des ensembles typés. Dans les graphes, le copier-coller conserve les groupes, les liaisons et les points de reroutage sélectionnés.
- **Markdown et chat.** L’éditeur natif propose les modes visuel, source, scindé et lecture. Il gère les tableaux, les pièces jointes et les documents `.md` et `.cymd`. La conversation virtualisée accepte des messages chargés à la demande. La sélection de texte et le retour à la ligne au bord de la barre de défilement ont été corrigés. Une démo SDL3 illustre ces usages.
- **Grilles et bureautique.** DataGrid, Spreadsheet et RichDocument ajoutent l’édition de cellules, les formules de tableur et les documents paginés. OfficeIO lit et écrit en mémoire un sous-ensemble des formats ODT, DOCX, ODS, XLSX, FODT et FODS et signale les pertes de conversion reconnues.
- **Terminal et outils d’inspection.** Le terminal interactif gère les séquences VT, la sélection et l’historique. L’exemple hôte le relie à un shell via ConPTY sous Windows ou un pseudo-terminal sous Linux et macOS. Les nouveaux widgets comprennent une vue hexadécimale paginée, la comparaison d’images, l’édition de dégradés, les résultats de recherche groupés et le suivi des tâches.
- **Documentation.** Le manuel CyDoc français/anglais indexe l’API publique et se consulte hors ligne. Il accompagne les nouvelles compilations de la démo, les paquets Python et Unreal et la vitrine statique bilingue.

### Autres correctifs

- Les interrupteurs peuvent recevoir le focus avec Tab et s’activer avec Espace. Entrée et Échap agissent sur la boîte de dialogue au premier plan lorsque plusieurs boîtes sont imbriquées.
- L’éditeur Markdown conserve la position du curseur et de la sélection autour des caractères UTF-8 multioctets, y compris les emoji, dans les modes source et scindé.
- Dans les grandes grilles, l’historique d’une modification de cellule conserve uniquement les données de la cellule modifiée. Les références entre feuilles suivent correctement les insertions de lignes et de colonnes et le renommage d’une feuille.

## Démo 1.2.0 publiée

La [publication v1.2.0](https://github.com/MrMybal/CyUI/releases/tag/v1.2.0) fournit
la galerie Windows et sa somme de contrôle SHA-256. Son contenu documenté inclut :

- Widgets natifs, dispositions, cinq thèmes et sélection anglais/français à chaud.
- Vues audio, graphiques, courbes éditables et chronologies.
- Graphes à nœuds avec aperçus synthétiques, navigateur de contenu et inspecteur de propriétés.
- Édition de code et modes syntaxiques, vues Diff, commandes et filtres de menus/panneaux.
- Licences des dépendances et informations sur la révision des sources.

[Téléchargez le ZIP Windows x64](https://github.com/MrMybal/CyUI/releases/download/v1.2.0/CyUI-Demo-1.2.0-windows-x64.zip)
et [sa somme de contrôle](https://github.com/MrMybal/CyUI/releases/download/v1.2.0/CyUI-Demo-1.2.0-windows-x64.zip.sha256).
Extrayez toute l’archive avant de lancer l’exécutable.

## Limites documentées

- **Plateformes :** l’exécutable publié est uniquement pour Windows x64. Les
  sources de la bibliothèque et la configuration CI existent pour Linux/macOS ;
  les vérifications locales Windows ne valident pas l’exécution sur ces systèmes.
  Le déploiement CEF nécessite une validation par cible.
- **Responsabilités de l’hôte :** graphes et vues affichent les données fournies
  par l’application. Décodage audio, FFT, DSP, exécution des shaders, opérations
  sur les fichiers, accès aux dépôts et services de langage restent dans l’application.
- **Éditeur de code natif :** CodeEditor demande C++20. Les modes syntaxiques
  fournissent une coloration lexicale, sans compilateur, débogueur ni serveur
  de langage. L’éditeur Python utilise une saisie multiligne avec aperçu
  syntaxique et n’expose pas tout le backend TextEditor natif.
- **Intégrations optionnelles :** la démo ordinaire n’embarque aucun runtime CEF
  ou Unreal. Le connecteur Unreal vise Editor C++/Slate, pas runtime/UMG.
  SDK externes, polices et notices des dépendances restent à la charge de l’hôte.
- **Documentation :** conservez tout le dossier `Documentation/`. Le lecteur
  utilise JavaScript ; préférences et annotations personnelles dépendent des
  autorisations de stockage du navigateur. Références externes et téléchargements
  nécessitent une connexion.

Consultez le [guide des plateformes](Documentation/Documentation.html#/fr/overview/platforms),
le [guide Web](Documentation/Documentation.html#/fr/web) et le
[guide Unreal](Documentation/Documentation.html#/fr/unreal) pour le périmètre documenté.

## Problèmes et retours

Consultez le [suivi des problèmes](https://github.com/MrMybal/CyUI/issues) pour
les signalements et échanges.
