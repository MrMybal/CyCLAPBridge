---
id: cli
title: Ligne de commande
description: Inspecter les modules, choisir un ID, générer des wrappers et reprendre une compilation.
section: Manuel
parent: overview
order: 20
sourceRevision: 1
---

## Examiner les capacités {#inspect}

```sh
cyclapbridge-cli capabilities
cyclapbridge-cli inspect --from jsfx --input "D:/Effects/Gain.jsfx"
```

L’inspection exécute le plugin ou le script d’entrée. Lancer cette commande dans l’environnement d’hébergement choisi pour le code étranger. La disponibilité des capacités décrit les directions d’intégration, sans promettre la compatibilité de tous les plugins tiers ou de toutes les plateformes cibles.

## Générer un projet {#convert}

```sh
cyclapbridge-cli convert --from clap --to lv2 --input "D:/Plugins/Gain.clap" --output "D:/Exports/Gain-LV2" --build
cyclapbridge-cli convert --from lv2 --to clap --input "D:/Plugins/Gain.lv2" --output "D:/Exports/Gain-CLAP" --build
cyclapbridge-cli convert --from jsfx --to vst3 --input "D:/Effects/Gain.jsfx" --resources "D:/Effects" --output "D:/Exports/Gain-VST3" --build
```

La sortie doit être un nouveau dossier. Sans `--build`, la commande génère seulement le projet. `--resources` copie une racine contenant l’entrée et les imports/assets, en conservant les chemins relatifs. La destination doit être extérieure. La génération des métadonnées LV2 charge le plugin choisi pendant la compilation.

## Choisir un plugin {#selection}

`--plugin-id` accepte un ID CLAP, une URI LV2 ou un ID de classe VST3 de 32 caractères hexadécimaux dans l’ordre natif des octets. Sans cette option, le premier plugin compatible est choisi. L’identité du wrapper dépend du format source, du nom d’entrée et de la sélection plutôt que du chemin absolu de la machine.

## Options et reprise de compilation {#options}

Utiliser `--name` pour le nom du résultat, `--backend-root` pour les sources installées et `--bundle-id` pour l’identité Apple. `--au-sdk` sélectionne AudioUnitSDK. Les options de compilation sont `--cmake`, `--generator`, `--arch` et `--config`. AUv3 demande le générateur Xcode sous macOS ; Windows ne compile pas les frameworks Apple.

```sh
cyclapbridge-cli build --project "D:/Exports/Gain-VST3"
```

Cela reprend un projet généré. Conserver son payload et ses notices. Voir [Distribution](doc:distribution) avant de déplacer les artefacts compilés.
