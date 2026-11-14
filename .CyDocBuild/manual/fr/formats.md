---
id: formats
title: Formats et limites
description: Comprendre les directions d’entrée/sortie et les éditeurs natifs représentés.
section: Manuel
parent: overview
order: 30
sourceRevision: 1
---

## Matrice de conversion {#matrix}

| Format | Entrée | Sortie | Limite de plateforme |
| --- | --- | --- | --- |
| CLAP | Oui | Oui | Hébergement direct et façades générées |
| VST3 | Oui | Oui | SDK MIT épinglé ; chemin hôte/éditeur Windows |
| LV2 | Oui | Oui | Métadonnées du bundle, audio, MIDI Atom et état |
| JSFX | Oui, source EEL2 | Pas de cible script autonome | Runtime ysfx ; `@gfx` Windows |
| AUv2 | Adaptateur hôte Apple | clap-wrapper | macOS requis ; intégration source non validée |
| AUv3 | Pas d’entrée de fichier arbitraire | Extension et conteneur | macOS/Xcode/signature requis ; non validé |

L’implémentation Apple est disponible dans les sources ; les tests natifs sont reportés car le propriétaire n’a pas de Mac. Les compilations Linux restent non validées. Studio est réservé à Windows. AAX ne possède aucune option d’entrée, de sortie ou de SDK.

## JSFX {#jsfx}

JSFX est un langage de scripts. Un script JSFX ordinaire ne charge pas de plugins natifs CLAP/VST3/LV2/AU. Une telle sortie demanderait une extension native REAPER ; CyCLAPBridge la refuse. Le runtime ysfx épinglé gère jusqu’à 64 sliders et 64 canaux. Conserver imports et ressources. Les fonctions propres à REAPER peuvent demander des services extérieurs à un hôte autonome.

## Contrat commun audio et éditeur {#common}

L’API commune représente les bus float32, les événements de paramètres, les notes et les messages MIDI 1. Les valeurs VST3 sont normalisées dans [0,1] ; les autres backends conservent leurs unités natives. Les événements non représentables sont refusés ou signalés par `outputEventsDropped`. Float64, SysEx, MIDI 2, modulation par voix et traduction complète du transport sont hors de ce contrat.

Les éditeurs embarqués couvrent la GUI native CLAP, VST3 HWND, LV2 WindowsUI et JSFX `@gfx` sous Windows. AU Cocoa est une intégration source non validée. L’adaptation LV2 Gtk/Qt, UI Atom/patch, le dépôt de fichiers JSFX et les éditeurs VST3/LV2/JSFX hors Windows restent hors du périmètre actuel. Voir [Éditeurs](doc:editors).
