---
id: studio
title: Créer un wrapper
description: Choisir une source, conserver ses ressources et compiler un nouveau projet de conversion.
section: Manuel
parent: overview
order: 10
sourceRevision: 2
---

```image
{"id":"studio-interface","src":"assets/studio-fr.png","alt":"Formulaire Studio avec sélection des formats source et cible","controls":"minimal"}
```

## Choisir l’entrée {#input}

1. Ouvrir Studio et sélectionner le format source à gauche et le format cible à droite.
2. Utiliser **Fichier** pour un fichier de plugin ou une source JSFX, ou **Bundle** pour un dossier LV2/VST3/AU.
3. Choisir un nouveau dossier de projet et saisir le nom du wrapper. Les dossiers de projet existants sont refusés pour éviter d’écraser des données.
4. Conserver le dossier des composants CyCLAPBridge détecté ou choisir le dossier installé contenant `third_party` et `templates`.

Studio peut générer des projets Apple ; les compiler demande macOS, et AUv3 demande Xcode. Sous Windows/Linux, utiliser Créer le projet pour ces cibles ; Créer et compiler est indisponible. Consulter [Formats](doc:formats) avant de choisir une direction.

## Inclure les ressources {#resources}

Le dossier facultatif de ressources/imports doit contenir l’entrée sélectionnée et ses fichiers associés. Leurs chemins relatifs sont conservés. Cela sert aux imports JSFX, aux images et aux ressources de plugins. Choisir une destination extérieure à ce dossier source. Les banques, les dépendances externes et les autorisations restent sous la responsabilité de l’éditeur du plugin.

## Générer et compiler {#build}

**Créer le projet** écrit un projet CMake sans ouvrir le plugin d’entrée. **Créer et compiler** lance aussi CMake et le compilateur sur un thread de fond ; consulter le journal en cas d’échec. Pour une sortie LV2, la compilation lance un outil de métadonnées séparé qui charge le plugin. Cet outil n’est pas une sandbox de sécurité.

Après réussite, **Ouvrir le résultat** ouvre le dossier des artefacts. Garder le wrapper, son `CyCLAPBridge-payload` et ses `CyCLAPBridge-licenses` ensemble. Rien n’est installé automatiquement dans les dossiers de plugins d’un DAW. Suivre la procédure normale de découverte du DAW pour installer le résultat complet.

## Choisir un plugin dans un module {#selection}

Studio utilise le premier plugin compatible d’un module multiple. Utiliser la [CLI](doc:cli#selection) avec `--plugin-id` lorsqu’un ID ou une URI précis est nécessaire. La CLI expose aussi les générateurs CMake, l’architecture, la configuration et les options de bundle Apple.

## Langue, thème et outils {#settings}

Choisir l’anglais ou le français et un thème en haut. Les préférences sont conservées dans le dossier de configuration de l’utilisateur du système. Le navigateur de fichiers commun utilise les racines locales ; les chemins complets peuvent aussi être collés dans les champs. Les options avancées sélectionnent les sources des composants, l’ID du plugin, l’exécutable CMake, le générateur, l’architecture et la configuration. CMake doit être installé ; les emplacements Windows/macOS courants sont détectés et un exécutable personnalisé peut être choisi.
