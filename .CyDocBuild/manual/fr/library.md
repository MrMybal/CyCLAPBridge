---
id: library
title: Intégrer la bibliothèque
description: Lier le paquet statique C++20 et gérer le cycle d’hébergement dans un DAW.
section: Référence développeur
order: 40
sourceRevision: 1
---

## Lier le paquet installé {#link}

```cmake
find_package(CyCLAPBridge 0.2 CONFIG REQUIRED)
target_link_libraries(MyDaw PRIVATE CyCLAPBridge::CyCLAPBridge)
```

Sélectionner le préfixe d’installation avec `CMAKE_PREFIX_PATH`. Les en-têtes publics sont `cyclapbridge/Conversion.h`, `cyclapbridge/Plugin.h` et `cyclapbridge/Editor.h` ; espace de noms `cybridge`. Le paquet est statique et garde les SDK audio privés. Il n’importe ni périphérique audio ni toolkit graphique.

## Cycle de vie et threads {#lifecycle}

Construire `HostedPlugin(Format, absolutePath, optionalPluginId)` sur le thread créateur/UI dans le processus choisi par le DAW. Lire `descriptor()`, `inputs()`, `outputs()` et `parameters()` avant la préparation. Allouer les buffers planaires et le stockage d’événements du DAW avant de démarrer l’audio.

Appeler `prepare(sampleRate, maximumFrames, maximumEvents)` sur le thread de contrôle. Un propriétaire audio unique et sérialisé appelle `start()`, les appels répétés à `process(ProcessBlock&)`, puis `stop()`. Une fois l’audio arrêté, le contrôle peut appeler `release()`, changer la configuration ou détruire l’instance. Ne pas faire chevaucher traitement, cycle de vie ou opérations d’état.

`ProcessBlock` contient le nombre de frames, les spans de bus, les événements d’entrée et une capacité d’événements de sortie appartenant à l’appelant. Vérifier `ProcessStatus`, `outputEventCount` et `outputEventsDropped`. Ses pointeurs/spans doivent rester valides pendant l’appel et locaux au processus d’hébergement. Le traitement préparé de l’adaptateur utilise un stockage borné ; le code arbitraire des DLL/scripts étrangers ne reçoit pas cette garantie temps réel.

## Requêtes et état {#state}

Appeler régulièrement `pumpMainThread()` sur le thread créateur et traiter les indicateurs retournés. Le redémarrage/rescan demande un cycle arrêté/désactivé selon le cas. Pour les instances CLAP activées, `flushRequested` et `processRequested` demandent au DAW de planifier le travail audio ; le pump UI ne remplace pas le propriétaire audio.

`latencyFrames()` indique la latence. `saveState(maximumBytes)` renvoie des octets opaques bornés ; `loadState(span)` les restaure avec l’audio arrêté. `HostedPlugin` inclut les valeurs UI en attente dans son enveloppe d’état. Conserver cet état comme des données opaques au lieu d’en interpréter la disposition. `parameterValue(id, value)` est une requête du thread créateur/UI.

## API de conversion {#conversion}

`backendFromRoot(path)` sélectionne les sources de conversion conservées. Remplir une `ConversionRequest`, puis appeler `createConversionProject(request)` sur un thread de contrôle/fond. Le résultat est un `ConversionProject` ; une entrée invalide, une direction non prise en charge ou une erreur d’E/S lève une exception. Les destinations existantes sont refusées.

`buildConversionProject(project, options, logSink)` lance CMake directement sans shell et renvoie un `BuildResult`. Génération et compilation restent hors du thread audio. Le DAW fournit la supervision des processus ou la frontière de sécurité. Voir [Ligne de commande](doc:cli) pour les options correspondantes et [Éditeurs](doc:editors) pour la propriété des GUI.
