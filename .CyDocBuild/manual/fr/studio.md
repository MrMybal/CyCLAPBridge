---
id: studio
title: Créer un wrapper
description: Choisir une source et livrer un plugin compilé dans un autre format.
section: Manuel
parent: overview
order: 10
sourceRevision: 4
---

```image
{"id":"studio-interface","src":"assets/studio-fr.png","alt":"Formulaire Studio avec sélection des formats source et cible","controls":"minimal"}
```

## Choisir l’entrée {#input}

1. Ouvrir Studio et sélectionner le format source à gauche et le format cible à droite.
2. Utiliser **Fichier** pour un fichier de plugin ou une source JSFX, ou **Bundle** pour un dossier LV2/VST3/AU.
3. Choisir un nouveau dossier de destination et saisir le nom du wrapper. Les dossiers existants sont refusés pour éviter d’écraser des données.
4. Conserver le dossier `Resources` de l’application à côté de l’exécutable ; les composants de conversion sont inclus.

La conversion Apple demande macOS, et AUv3 demande Xcode. Ces cibles sont indisponibles dans le Studio Windows/Linux. La validation Apple reste reportée. Consulter [Formats](doc:formats) avant de choisir une direction.

## Inclure les ressources {#resources}

Le dossier facultatif de ressources/imports doit contenir l’entrée sélectionnée et ses fichiers associés. Leurs chemins relatifs sont conservés. Cela sert aux imports JSFX, aux images et aux ressources de plugins. Choisir une destination extérieure à ce dossier source. Les banques, les dépendances externes et les autorisations restent sous la responsabilité de l’éditeur du plugin.

## Générer et compiler {#build}

Choisir **Convertir**. Studio prépare ses composants, réalise la conversion en arrière-plan et livre le plugin compilé, son payload et ses notices dans le dossier de destination. Consulter le journal de conversion en cas d’échec. L’application est déjà compilée ; la conversion des plugins demande encore CMake 3.27+ et un compilateur C++20 natif. Sous Windows, installer Visual Studio 2022 avec Développement Desktop en C++ et un SDK Windows. Les emplacements CMake courants sont détectés automatiquement.

Pour une sortie LV2, la conversion lance un outil de métadonnées qui charge le plugin. Cet outil n’est pas une sandbox de sécurité.

Après réussite, **Ouvrir le résultat** ouvre le dossier des artefacts. Garder le wrapper, son `CyCLAPBridge-payload` et ses `CyCLAPBridge-licenses` ensemble. Rien n’est installé automatiquement dans les dossiers de plugins d’un DAW. Suivre la procédure normale de découverte du DAW pour installer le résultat complet.

## Choisir un plugin dans un module {#selection}

Studio utilise par défaut le premier plugin compatible d’un module multiple. Saisir son ID CLAP, son URI LV2 ou son ID de classe VST3 dans **ID du plugin (facultatif)** sous **Options avancées** pour sélectionner un plugin précis. La [CLI](doc:cli#selection) propose l’option équivalente `--plugin-id` ainsi que des options supplémentaires de bundle Apple.

## Langue, thème et outils {#settings}

Choisir l’anglais ou le français et un thème en haut. Les préférences sont conservées dans le dossier de configuration de l’utilisateur du système. Le navigateur de fichiers commun utilise les racines locales ; les chemins complets peuvent aussi être collés dans les champs. Les options avancées sélectionnent l’ID du plugin et permettent d’adapter CMake, le générateur, l’architecture et la configuration au besoin.
