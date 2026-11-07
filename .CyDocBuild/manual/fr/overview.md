---
id: overview
title: Manuel CyUI
description: Un socle Dear ImGui commun pour les outils de bureau C++ et Python.
section: Découvrir
order: 0
sourceRevision: 1
---

CyUI est une bibliothèque de widgets thématisés construite sur **Dear ImGui 1.92+**, avec des modules C++ et Python sélectionnables. Elle s’adresse aux développeurs d’éditeurs, d’outils créatifs et d’utilitaires de bureau qui recherchent contrôles cohérents, localisation et vues spécialisées. L’hôte conserve moteur de rendu, données, moteurs métier et historique Undo.

```image
{"id":"workspace","src":"assets/overview-fr.webp","alt":"Démo CyUI avec galerie ancrée, propriétés et panneaux d’outils en français","controls":"minimal"}
```

## Premiers pas {#getting-started}

- [Intégrer CyUI](doc:integration) avec votre cible ImGui existante.
- [Choisir les modules](doc:modules) et respecter [le contrat commun d’édition](doc:modules#shared-data-and-edit-contract).
- Découvrir [les widgets Core](doc:widgets), [les recettes des modules](doc:module-recipes), [la localisation](doc:localization) et [les filtres rapides](doc:filtering).
- Utiliser [Python](doc:python), [Web/CEF](doc:web) ou le [connecteur Unreal Editor](doc:unreal).
- Lire [les licences](doc:licensing) avant de redistribuer les dépendances.

## Explorer la démo {#demo}

Extrayez toute l’archive portable Windows, puis lancez `cyui_demo.exe`. L’anglais est le défaut ; `Launch-French.cmd` choisit le français. La galerie permet de changer immédiatement thème, densité et langue. Ses données sont synthétiques et ses aperçus de textures générés. Mixeur, transport et panneaux illustrent l’interface ; ils ne constituent pas un moteur audio, un système de projet ou un éditeur complet.

```sh
cyui_demo.exe --locale fr
cyui_demo.exe --theme violet --comfortable
cyui_demo.exe --gallery-only --tab 21
cyui_demo.exe --gallery-only --tab 7 --image-graph
```

La galerie native propose Texte, Boutons, Contrôles, Conteneurs, Canevas, Tracés, Courbes, Nœuds, Timeline, Vues audio, Code, Diff, Détails, Assets, Images, Viewport, Commandes, Diagnostics, Documents, Web, Disposition et Filtres. Le panneau Web nécessite un backend CEF configuré séparément pour naviguer ; la démo ordinaire téléchargeable n’embarque ni CEF ni Unreal.

## Plateformes et statut {#platforms}

La version actuelle est **1.2.0**. Le projet ne déclare pas de label alpha/bêta et son développement est actif.

| Composant | Disponibilité |
| --- | --- |
| Core et modules ordinaires C++ | Sources portables Windows/Linux/macOS ; validation locale MSVC/rendu Windows et CI multiplateforme configurée |
| Démo autonome | Windows x64, Win32/DirectX 11 ; aucun binaire de démo Linux/macOS |
| Python | Python 3.10+, imgui-bundle pour dessiner ; imports de modèles indépendants du rendu |
| Adaptateur CEF | SDK/runtime optionnel ; validation locale Windows, déploiements Linux/macOS à vérifier sur cible |
| Connecteur Unreal | UE 5.5+ Editor C++ optionnel ; validation locale UE 5.8.4 Windows, autres cibles à vérifier |

Core utilise C++17. CodeEditor et l’intégration du SDK CEF testé demandent C++20. La démo Windows utilise les polices système ; les hôtes portables fournissent des polices couvrant leurs langues. Voir [initialisation](doc:integration#startup) et [limites de localisation](doc:localization#what-applications-translate).

## Lire hors ligne {#offline-help}

Ouvrez directement **Documentation/Documentation.html** depuis le disque. Conservez tout le dossier Documentation, dont DocData. Aucun serveur, CDN, compte ou accès Internet n’est requis. Les références externes nécessitent une connexion seulement si vous choisissez de les suivre.

F1 et **Aide → Documentation** dans la démo ouvrent ce manuel local dans la langue de l’interface. Le menu propose aussi modules, filtres et localisation. Dans la galerie Python, le bouton Documentation ou F1 ouvre l’aide du module actif. [L’intégration de l’aide](doc:help) décrit le helper Python et les routes natives réutilisables.

Utilisez l’arborescence latérale ou Ctrl+K pour chercher dans les textes, exemples de code et symboles API documentés. Le changement de langue conserve les identifiants de page et section. Images et galeries s’agrandissent **dans la fenêtre du manuel** ; Échap referme cet agrandissement. Le clavier révèle les contrôles, le tactile les conserve visibles, et la navigation en session mémorise les positions de lecture. Annotations/préférences personnelles dépendent des autorisations de stockage local du navigateur.

[Visiter les modules en images](doc:visual-tour).
