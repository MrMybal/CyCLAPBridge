---
id: distribution
title: Distribuer un wrapper ou la bibliothèque
description: Garder ensemble payloads, ressources nécessaires et notices des composants.
section: Manuel
parent: overview
order: 60
sourceRevision: 1
---

## Conserver le résultat complet {#files}

Garder le wrapper avec son `CyCLAPBridge-payload`, ses ressources et ses `CyCLAPBridge-licenses`. Certains formats placent ces fichiers dans le bundle. Déplacer uniquement la DLL ou le plugin extérieur peut empêcher le chargement. Les banques externes, runtimes et permissions des éditeurs restent applicables. CyCLAPBridge n’installe pas automatiquement les plugins.

La bibliothèque statique installée comprend les notices dans `share/CyCLAPBridge/licenses`, même avec le backend source désactivé. Distribuer ces notices avec les binaires qui intègrent la bibliothèque. Conserver l’export `Documentation` complet lors de la distribution du manuel.

## Licences {#licenses}

Le code original de CyCLAPBridge est sous MIT. Le SDK VST3 3.8.0 build 66 épinglé et ses composants imbriqués sont sous MIT ; VST3 n’impose pas la GPL à ce projet. AudioUnitSDK et ysfx sont sous Apache-2.0. LV2, Lilv, Serd, Sord, Sratom et Zix conservent leurs notices ISC. WDL/LICE, les outils image/audio embarqués et les autres composants gardent leurs conditions distinctes.

Le parseur EEL2 généré conserve GPL-2.0-or-later avec Bison-exception-2.2. Cette exception permet à un ensemble plus vaste contenant le squelette du parseur d’utiliser des conditions distinctes dans cette application. Conserver la notice du parseur. Le tableau des licences du README racine et `THIRD_PARTY.md` indexent les notices complètes conservées et les révisions exactes.

Le lecteur de manuel distinct utilise CyDoc sous MIT. Ses notices, ainsi que celles de marked, PrismJS et esbuild utilisés à la génération, sont dans `DocData/licenses`.

## Droits du plugin sélectionné {#payload}

Créer un wrapper ne donne aucun droit de redistribution et ne remplace pas la licence du plugin choisi. Un payload GPL peut ajouter des obligations de distribution ; un payload propriétaire conserve ses restrictions. Garder les copyrights, attributions et textes de licence applicables à chaque ressource ou script inclus.
