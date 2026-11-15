---
id: overview
title: CyCLAPBridge
description: Choisir entre créer un wrapper et intégrer un hôte de plugins.
section: Manuel
order: 0
sourceRevision: 2
---

## Commencer {#getting-started}

CyCLAPBridge 0.2.0 est en développement. Il réunit une bibliothèque statique C++20 d’hébergement, une CLI de conversion et un Studio de bureau CyUI. Il est indépendant des autres projets Cy et des toolkits graphiques tiers.

Pour utiliser un plugin ou un script dans un autre format, suivre [Créer un wrapper](doc:studio). Pour réaliser un DAW, commencer par [Intégrer la bibliothèque](doc:library). La [matrice des formats](doc:formats) explique les directions prises en charge et leurs limites.

## Ouvrir les outils {#tools}

Après compilation et installation des sources, lancer `bin/CyCLAPBridgeStudio.exe` ou `bin/cyclapbridge-cli.exe` dans l’installation. Studio propose l’anglais par défaut et le français, des thèmes intégrés et un navigateur de fichiers commun. Appuyer sur F1 ou choisir son bouton d’aide pour ouvrir ce manuel. Le sélecteur de langue donne accès à l’édition anglaise complète.

Le manuel s’ouvre directement depuis `Documentation.html` ; conserver son dossier `DocData` à côté. Rechercher depuis l’en-tête ou avec Ctrl+K. L’arbre de navigation regroupe les tâches et les références. Aucun environnement Node.js ni serveur web n’est nécessaire à la lecture.

## Comprendre le wrapper {#wrapper}

Un wrapper adapte une API autour d’un binaire de plugin existant ou d’une source JSFX. Il conserve le payload original et ses ressources ; il ne traduit pas le code machine et ne change pas sa licence. Garder les artefacts générés et les notices ensemble. Voir [Distribution](doc:distribution).

## Processus d’hébergement {#process}

Le DAW choisit le processus, crée et supervise les workers, transporte l’audio et les événements par IPC, récupère après les crashes et fournit l’éventuelle sandbox de sécurité. CyCLAPBridge s’exécute dans ce processus choisi ; il ne crée pas cette isolation. Ses instances C++, handles et spans audio restent locaux à ce processus. Les plugins et scripts étrangers conservent leur comportement temps réel.

## Applications par plateforme {#platforms}

Sous Windows, lancer `bin/CyCLAPBridgeStudio.exe` ; sous Linux, `bin/CyCLAPBridgeStudio`. L’installation macOS préparée contient `CyCLAPBridgeStudio.app`. Les compilations natives Linux/macOS restent à valider. Conserver l’installation complète. L’application Mac stocke le manuel et les ressources de conversion dans `Contents/Resources/CyCLAPBridge`.
