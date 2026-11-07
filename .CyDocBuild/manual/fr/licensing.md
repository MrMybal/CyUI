---
id: licensing
title: Licences et distribution commerciale
description: Conditions de CyUI et des dépendances effectivement redistribuées.
section: Intégrations
order: 36
sourceRevision: 1
---

CyUI est sous licence MIT, y compris ses modules et adaptateurs CEF/ImGuizmo écrits dans le projet. Elle autorise utilisation commerciale, modification, redistribution et sous-licence en conservant les mentions de copyright et d’autorisation. Elle n’impose pas de publier les sources de l’application.

Conservez LICENSE, THIRD_PARTY_NOTICES.md et les notices des dépendances effectivement distribuées. Dear ImGui, TextEditor et les headers stb dans ImGui gardent leurs conditions MIT (ou domaine public pour l’alternative stb). Sans CYUI_IMGUI_TARGET hôte, CMake lie l’ImGui embarqué. Les dépendances externes gardent leur licence propre ; elles ne sont pas relicenciées par CyUI.

CEF exige LICENSE.txt, crédits Chromium et notices de son build exact. Vérifiez séparément les codecs/médias supplémentaires choisis par l’hôte. CEF n’est pas un moteur placé sous MIT par CyUI.

Les frontends et applications tierces ont des licences indépendantes, dont parfois GPL/AGPL. Leur chargement ou redistribution ne change pas leur licence en MIT. Le bridge est un transport de messages et une implémentation du protocole documenté d’intégration CyMD ; il ne donne pas de droits sur l’application distante. Décidez quelles applications empaqueter selon leurs conditions.

Les présentations partagées sont écrites contre des modèles/callbacks neutres. TextEditor est un instantané MIT avec licence et hachages. Le lecteur CyDoc est MIT ; son texte de licence est fourni dans DocData/CyDoc-LICENSE.txt. Les images de ce manuel montrent les exemples synthétiques de la galerie.
