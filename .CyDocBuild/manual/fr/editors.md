---
id: editors
title: Embarquer un éditeur de plugin
description: Posséder les fenêtres parentes natives, traiter le travail UI et relayer l’automation.
section: Référence développeur
parent: library
order: 50
sourceRevision: 1
---

## Propriété du parent natif {#parent}

Le DAW possède la fenêtre parente. Les appels d’éditeur et `pumpMainThread()` s’exécutent sur le thread créateur/UI de l’instance ; sous macOS, il doit aussi être le thread principal Cocoa. Aucun SDK ni framework graphique n’est requis dans les en-têtes publics.

Appeler `editorSupported(api)`, puis `createEditor(api, callbacks)`. Interroger `editorSize()`, dimensionner le parent et l’attacher avec `setEditorParent({api, nativeParent})`. L’afficher avec `showEditor(true)`. Si l’attachement échoue, détruire l’éditeur. Appeler `destroyEditor()` avant de détruire son parent ou son plugin.

`EditorCallbacks` contient un pointeur de contexte et des callbacks noexcept pour les demandes de taille, les gestes de paramètres et la fermeture. Garder le contexte valide jusqu’au retour de destruction de l’éditeur. Le callback de taille renvoie false si le DAW refuse la dimension. Des méthodes facultatives exposent contraintes, tailles ajustées et prise en charge de l’échelle ; ne pas supposer que chaque plugin les accepte.

## Pump et automation {#automation}

Traiter le travail UI environ toutes les 33 ms tant que l’éditeur est ouvert. Cela sert les timers CLAP, l’idle LV2, les graphismes JSFX et la synchronisation des paramètres. L’ouverture/fermeture peut coexister avec l’audio ; préparation et sauvegarde/restauration restent des opérations de contrôle avec l’audio arrêté.

Les valeurs UI sont coalescées par paramètre et fusionnées au début du bloc audio suivant. Un événement du DAW à la frame zéro prend le pas sur une valeur UI en attente du même bloc. Les gestes CLAP en sortie utilisent `EventKind::gestureBegin` et `gestureEnd` ; ils ne sont pas des entrées audio de l’API commune.

La façade CLAP générée demande `CLAP_EXT_TIMER_SUPPORT` à l’hôte pour son éditeur. Sa file bornée de gestes contient 1023 entrées utilisables ; un débordement demande un rescan des valeurs. Les valeurs audio restent coalescées. Le DAW planifie toujours le traitement audio demandé.

## Limites des backends {#backends}

- CLAP relaie la GUI embarquée compatible fournie par le plugin.
- VST3 utilise `IPlugView`/`IPlugFrame` sur HWND ; le support DPI dépend du plugin.
- LV2 prend en charge WindowsUI, les contrôles float et les fonctions hôte acceptées, sans adaptation Gtk/Qt ni UI Atom/patch.
- Les scripts JSFX avec `@gfx` utilisent ysfx/WDL/LICE sous Windows, avec souris, clavier et menus. Sans `@gfx`, aucun éditeur natif n’est fourni ; le DAW peut afficher les sliders. Le dépôt de fichiers est indisponible.
- Les sources de l’éditeur AU Cocoa sont intégrées, sans validation macOS. Les éditeurs VST3/LV2/JSFX hors Windows ne sont pas annoncés.

Les plugins et scripts conservent leurs dépendances GUI, allocations, synchronisations et accès aux fichiers. Voir [Formats](doc:formats) et [Propriété des processus](doc:overview#process).
