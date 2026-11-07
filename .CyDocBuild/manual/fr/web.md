---
id: web
title: Panneaux Web avec CEF
description: Intégrer un frontend HTML/JavaScript avec un runtime explicitement optionnel.
section: Intégrations
order: 34
sourceRevision: 1
---

Web affiche des interfaces HTML/CSS/JavaScript dans un panneau Dear ImGui. Le build du frontend compile TypeScript en JavaScript. L’adaptateur fourni utilise **CEF** en rendu hors écran vers une texture de l’hôte, sans fenêtre native recouvrant popups/docks. Core ne dépend d’aucun navigateur.

## Plateformes {#platform-and-validation-matrix}

| Composant | Windows | Linux | macOS |
| --- | --- | --- | --- |
| Core et modules ordinaires C++ | Sources portables, build/rendu MSVC validés localement | Sources portables, CI configurée, pas d’exécution locale | Idem |
| Python, modèles et UI imgui-bundle | Modèles et frames réelles vérifiés | Environnement cible à valider | Environnement cible à valider |
| Politique Web, messages, JS/TS | Vérifiés | Code indépendant de l’OS | Code indépendant de l’OS |
| Adaptateur WebCEF hors écran | Compilation, page, peinture et messages natifs/Python vérifiés | Build/runtime et distribution à valider | Framework/helper/AppKit et distribution à valider |
| Démo Win32/DX11 et captures BMP | Implémentée | Backend ImGui fourni par l’hôte | Backend ImGui fourni par l’hôte |

CEF propose des distributions Windows/Linux/macOS ; une option CMake ne valide pas une application complète. Le SDK utilisé localement était `150.0.23+g4ccc4ca+chromium-150.0.7871.287`, Windows x64. Choisissez et validez un SDK maintenu pour chaque architecture cible.

## Sélection du build {#build-selection}

```cmake
set(CYUI_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(CYUI_MODULES "Web" CACHE STRING "" FORCE)
set(CYUI_BUILD_WEB_CEF ON CACHE BOOL "" FORCE)
set(CYUI_CEF_ROOT "path/to/extracted/cef_binary_distribution" CACHE PATH "" FORCE)
add_subdirectory(external/CyUI)
target_link_libraries(app PRIVATE CyUI::WebCEF)
```

Vous pouvez définir `CYUI_CEF_TARGET` sur une cible hôte fournissant en-têtes, runtime et wrapper C++. L’adaptateur demande C++20 avec le SDK testé ; Core reste C++17. Configure et rendu ne téléchargent rien. Le build crée aussi `cyui_webcef_bridge`, bibliothèque C ABI, et `cyui_webcef_helper` pour Python/autres hôtes. Si l’application initialise déjà CEF, réutilisez son cycle de processus au lieu de créer un second runtime.

## Cycle de vie natif {#native-lifecycle}

1. Passez `webCEFApp()` à CefExecuteProcess et CefInitialize avant les vues. Tous les helpers utilisent le même handler renderer.
2. Activez `CefSettings.windowless_rendering_enabled`. L’hôte possède initialisation, sandbox, profil/cache et chemins des sous-processus.
3. Fournissez upload pour un tampon BGRA8 d’origine supérieure gauche, valable durant le callback seulement. Mettez à jour une texture réutilisable et retournez son ImTextureID. L’hôte en reste propriétaire. release est appelé une fois après fermeture avec la dernière texture uploadée ; attendez que les frames soumises ne la référencent plus avant de la libérer.
4. Appelez CefDoMessageLoopWork une fois par boucle, **avant NewFrame et hors frame ImGui**. upload/release y sont exécutés sans concurrencer la construction de draw lists. Ensuite : NewFrame, beginFrame, webPanel, endFrame, Render. endFrame masque les panneaux non dessinés.
5. close toutes les vues et continuez de pomper jusqu’à closed. Détruisez-les avant les ressources GPU, puis CefShutdown sur le thread d’initialisation.

Tous les appels backend appartiennent au thread UI CEF qui a appelé CefInitialize avec la boucle monothread. Les autres appels sont ignorés/retournent false, la construction lève une exception ; une assertion existe en debug.

```cpp
#include <cyui/adapters/WebCEF.h>

cyui::WebCEFOptions options;
options.url = "http://127.0.0.1:4173/chat";
options.policy.origins = {"http://127.0.0.1:4173"};
options.upload = [&](int width, int height, const void* bgra) {
    return renderer.updateBrowserTexture(width, height, bgra);
};
options.release = [&](ImTextureID texture) { renderer.releaseAfterFramesInFlight(texture); };
auto web = cyui::createWebCEF(options); // after CefInitialize

// Each host loop iteration, on the same thread:
CefDoMessageLoopWork();       // outside the ImGui frame
ImGui::NewFrame();
web->beginFrame();
if (ImGui::Begin("Chat")) {
    for (const auto& event : cyui::webPanel("##chat", *web, {0, 0})) {
        if (event.kind == cyui::WebEventKind::navigated)
            web->postJson(cyui::webThemeJson());
        // Parse/validate application messages; apply host commands explicitly.
    }
}
ImGui::End();
web->endFrame();
ImGui::Render();
```

Le chemin de peinture logiciel borne chaque vue à 4096×4096 et 30 fps par défaut (limites configurables jusqu’à 8192). Les listes HTML sont composées dans la même texture. webPanel transmet boutons/clics multiples, deux molettes, Unicode, lettres/chiffres, F1–F24, ponctuation, pavé numérique, navigation/édition et modificateurs gauche/droite. Windows ajoute le scan code du clavier actif pour KeyboardEvent.code. Le panneau focalisé possède ces touches : Tab, flèches, Espace, Entrée et Échap ne naviguent pas aussi dans ImGui. WebPanelOptions.focus ne prend le focus qu’au passage false→true. L’hôte garde la saisie de texte de plateforme active.

Pour les événements natifs, composition IME/position des candidats, accessibilité, dépôt de fichiers OS et autres intégrations avancées, utilisez webCEFBrowser et les API CEF : ces fonctions relèvent de l’hôte et ne sont pas établies par les tests basiques. Ne combinez pas la boucle monothread avec le mode multi_threaded_message_loop ; le travail moteur parallèle reste dans les sous-processus CEF.

## Frontière du navigateur {#browser-boundary}

WebPolicy accepte des origines HTTP(S) exactes, port compris. Elle rejette identifiants dans l’URL, file/javascript, jokers, ports mal formés et antislashs ambigus. Servez votre frontend local sur loopback ou autorisez explicitement une origine HTTPS. CyUI ne démarre pas de serveur et ne choisit ni build TypeScript ni authentification. Les sous-ressources utilisent le réseau normal de CEF.

Les navigations principales hors liste et nouvelles fenêtres deviennent des événements external, jamais des ouvertures automatiques. Seuls les schemes externalSchemes sont rapportés (http/https par défaut). Les autres sont abandonnés, dont file et protocoles système, sauf ajout délibéré. Les popups de sous-frames sans geste utilisateur sont ignorés ; les événements portent userGesture/mainFrame/sourceOrigin. Avant un appel ShellExecute/open/navigateur, contrôlez votre liste d’autorisation et demandez confirmation. Ne passez pas une URL de page au shell sans validation.

Les messages proviennent uniquement de la frame principale d’une origine permise et sont bornés avant parsing : 1 Mio par défaut, au plus 64 Mio. postJson est associé à l’origine au moment de l’envoi ; une navigation ultérieure annule la livraison. La file a maxQueuedEvents=256 et un plafond séparé maxQueuedErrors=16 pour erreurs/console. Les événements dropped comptent saturation, dépassement de taille et refus de politique. Téléchargements et demandes de permissions/médias sont refusés par l’adaptateur minimal ; les capacités supplémentaires demandent une intégration hôte explicite.

### Sandbox des processus {#process-sandbox}

**Le bridge et le helper fournis ne sandboxent pas CEF sous Windows.** Leur initialisation ne reçoit pas sandbox_info et force no_sandbox=true. La demande Python no_sandbox=False est signalée mais non honorée : cyui_cef_sandboxed retourne 0, cyui_cef_error l’explique et CEFRuntime émet RuntimeWarning avec sandboxed=False. Sous Linux, la demande est transmise à Chromium et nécessite chrome-sandbox ou user namespaces ; sous macOS le helper doit aussi initialiser CefScopedSandboxContext. Ces deux cibles restent à valider.

Pour sandboxer un hôte Windows avec le SDK CEF 150 testé, utilisez son bootstrap.exe/bootstrapc.exe renommé en application, compilez l’application et l’entrée de sous-processus en DLL exposant RunWinMain/RunConsoleMain, transmettez sandbox_info à CefExecuteProcess et CefInitialize et gardez no_sandbox=false. Les anciens SDK avec cef_sandbox.lib utilisent CefScopedSandboxInfo dans wWinMain. Navigateur et helpers doivent suivre le même agencement, à valider sur la cible.

## JavaScript et TypeScript {#browser-javascript-typescript}

`web/` fournit le paquet MIT `@cyui/web`, ses déclarations TypeScript et une feuille CSS optionnelle. CEF installe le transport étroit window.cyuiNative ; il n’expose ni Electron ni Node à la page.

```ts
import {createCyUIBridge} from '@cyui/web';
const bridge = createCyUIBridge();
bridge.on('host.reply', payload => updateView(payload));
bridge.send('chat.message', {text: 'Hello'});
// Dispose listeners when the embedded UI is destroyed.
bridge.close();
```

L’enveloppe est `{protocol:'cyui.web', version:1, type, payload, requestId?}`. L’application possède schéma, autorisation et effets. Le thème fournit les variables CSS --cyui-bg0/--cyui-text/--cyui-accent ; renvoyez-le après changement de thème. Le JSON reste une donnée transportée par messages CEF, sans interpolation comme JavaScript exécutable.

## Frontends intégrés {#embedded-web-frontends}

mountCyMD implémente le protocole iframe version 1 : origine du parent, identité de frame/session, messages ready/open/save/state, documents Blob et accusés de sauvegarde. L’hôte fournit onSave, CyUI n’écrit pas de fichier. JSON natif ne transporte pas directement des Blob ; utilisez la page hôte et un protocole borné pour les documents. Les frontends gardent leurs services, licences et adaptateurs ; remplacez les API Electron preload/IPC par des messages explicites si nécessaire.

## Python {#python}

```python
from cyui_modules.cef import CEFRuntime
from cyui_modules.web import web_panel, theme_message

with CEFRuntime(library_path, helper_path, absolute_cache_path, no_sandbox=True) as runtime:
    view = runtime.create(url, (allowed_origin,), upload_bgra_to_host_texture,
                          release=release_texture_later)
    # Each loop iteration:
    runtime.pump()            # before new_frame: uploads/releases happen here
    imgui.new_frame()
    runtime.begin_frame()
    events = web_panel('chat', view, (0, 400))
    for event in events:
        if event['kind'] == 'navigated':
            view.post_json(theme_message())
    runtime.end_frame()
    imgui.render()
```

Tous les appels appartiennent au thread UI propriétaire. begin_frame ne pompe pas : appelez pump hors frame. Le context manager ferme toutes les vues et pompe avant shutdown ; celui-ci échoue s’il reste une vue, et CEF ne peut pas être réinitialisé dans le même processus. Le pointeur reçu par upload est temporaire : copiez/mettez à jour immédiatement. release est appelé par pump après fermeture. no_sandbox=True exprime le comportement Windows ; le défaut émet un avertissement. Le garbage collector ne doit pas initialiser/arrêter le navigateur depuis un thread arbitraire.

## Distribution {#deployment-and-checks}

webThemeJson/theme_message transportent aussi la langue active, anglais par défaut. Renvoyez le thème après changement de langue. Le bridge traduit les éléments marqués et expose le catalogue aux composants TypeScript. Distribuez cyui-i18n.js/cyui-locales.js avec le bridge principal. Voir [localisation](doc:localization).

- Windows : fichiers libcef/Chromium correspondants, ressources/locales, helper et configuration sandbox adaptée. Le staging optionnel CMake copie Release/Resources du SDK.
- Linux : libcef.so, ressources/locales, helper, chemins de chargement, affichage et sandbox Chromium.
- macOS : Chromium Embedded Framework.framework, helpers app correctement nommés/signés, AppKit, entitlements et sandbox. Le helper générique n’est pas un bundle macOS complet.
- Conservez LICENSE.txt, CREDITS.html et notices du SDK exact, ainsi que les conditions des codecs choisis. Voir [licences](doc:licensing).

La démo compilée avec CEF accepte `--web-url http://127.0.0.1:PORT/examples/web/index.html --tab 19`. Elle ne fournit pas sandbox_info : le paramètre --cef-no-sandbox ne la transforme pas en hôte sandboxé. La documentation CyDoc, elle, s’ouvre séparément dans le lecteur local et n’utilise pas la politique HTTP du module Web.
