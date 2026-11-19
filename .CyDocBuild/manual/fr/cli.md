---
id: cli
title: Ligne de commande
description: Inspecter les modules, choisir un ID, générer des wrappers et reprendre une compilation.
section: Manuel
parent: overview
order: 20
sourceRevision: 2
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

## Appeler les outils de Studio {#tools}

```sh
cyclapbridge-cli tools list
cyclapbridge-cli tools describe convert_plugin
cyclapbridge-cli tools call list_formats
```

Ces commandes écrivent un objet JSON sur la sortie standard. **List** renvoie `tools`, **describe** renvoie `tool` et **call** renvoie `result`. Les codes de sortie sont `0` pour la réussite, `2` pour une syntaxe ou un JSON invalide, `3` pour une erreur d’outil et `4` pour une erreur de connexion ou de protocole. Les erreurs sont renvoyées dans un objet `error`. Un retour réussi à l’état `awaiting_approval` indique que la demande est en attente. Utiliser `get_job` avec son `job_id` pour lire le résultat final.

Sans URL de Studio, la CLI décrit tout le registre et exécute `list_formats`. Les autres outils utilisent les fichiers, validations et jobs du Studio ouvert. Pour cibler une instance, démarrer son serveur **Assistants externes (MCP)** puis copier la connexion. Passer l’URL copiée dans `--mcp-url` et placer le jeton Bearer dans `CYCLAPBRIDGE_MCP_TOKEN`. Utiliser `--mcp-token-env NOM` pour lire une autre variable d’environnement. Fournir un objet d’arguments avec `--args` ou un fichier avec `--args-file`. L’URL doit être l’adresse locale donnée par l’instance Studio.

Les commandes `capabilities`, `inspect`, `convert` et `build` gardent leurs options et sorties existantes. Elles s’exécutent directement sans Studio.

Avec la CLI fournie dans le ZIP Studio, la première conversion extrait les composants correspondants dans le dossier utilisateur privé CLIBackend. Un projet généré retrouve ces composants lors d’une compilation ultérieure. L’option --backend-root continue de sélectionner les sources indiquées.
