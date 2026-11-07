---
id: diff
title: Comparaison de code native
description: Préparer un diff et naviguer dans des colonnes synchronisées.
section: Guides
parent: code
order: 31
sourceRevision: 1
---

Sélectionnez `Diff` dans `CYUI_MODULES`, incluez `<cyui/modules/Diff.h>` et liez `CyUI::Diff`. Le module dépend de Core et Code, avec l’ImGui déjà présent dans l’hôte. Il n’ajoute ni backend de plateforme, ni Git, ni éditeur.

Préparez un `DiffDocument` avec `setTexts` ou `setUnified`, gardez un `DiffState` par vue et appelez `diffViewer(id, state, document, size)`. L’API et la structure des états/résultats restent compatibles. La préparation appartient au document : redessiner une vue préparée ne recalcule pas le diff.

Les deux colonnes sont peintes dans un seul canevas enfant, à la même origine de ligne visible. Molette, trackpad et barre verticale déplacent ce canevas une seule fois, depuis l’une ou l’autre colonne. Aucun défilement n’est recopié avec une frame de retard. Les lignes vides jumelées alignent ajouts et suppressions ; l’espacement fixe correspond au clipper pour conserver la navigation dans les grands fichiers.

Les longues lignes partagent un seul décalage horizontal. Les numéros restent fixes, le code et les différences internes sont découpés dans chaque colonne. Les largeurs suivent le canevas visible : une ligne longue ne pousse pas l’autre colonne hors écran. Le mode unifié utilise le même canevas. Navigation par bloc/recherche, sélection, libellés traduits, couleurs syntaxiques et copie de ligne restent disponibles.

`DiffState::scroll` expose la position verticale après saisie ; lui affecter une nouvelle valeur la restaure à l’appel suivant. `focus` demande la navigation vers une ligne ou un bloc. Des IDs distincts isolent les états. Changer de disposition ne crée pas un autre enfant ImGui et ne double pas les entrées.

Voir [la recette et les limites de préparation](doc:module-recipes#code-editing-and-git-style-diff-presentation). Le rendu Python historique est une implémentation indépendante.

```image
{"id": "diff-view", "src": "assets/diff.webp", "alt": "Comparaison côte à côte alignée", "controls": "minimal"}
```
