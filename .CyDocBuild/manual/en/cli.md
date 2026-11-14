---
id: cli
title: Command line
description: Inspect modules, select plugin IDs, generate wrappers and resume builds.
section: Manual
parent: overview
order: 20
revision: 1
---

## Inspect capabilities {#inspect}

```sh
cyclapbridge-cli capabilities
cyclapbridge-cli inspect --from jsfx --input "D:/Effects/Gain.jsfx"
```

Inspection executes the input plugin or script. Run it in the hosting environment you selected for foreign code. Capability availability describes integration directions, not a promise that every third-party plugin or target platform is compatible.

## Generate a project {#convert}

```sh
cyclapbridge-cli convert --from clap --to lv2 --input "D:/Plugins/Gain.clap" --output "D:/Exports/Gain-LV2" --build
cyclapbridge-cli convert --from lv2 --to clap --input "D:/Plugins/Gain.lv2" --output "D:/Exports/Gain-CLAP" --build
cyclapbridge-cli convert --from jsfx --to vst3 --input "D:/Effects/Gain.jsfx" --resources "D:/Effects" --output "D:/Exports/Gain-VST3" --build
```

The output must be a new directory. Without `--build`, the command only generates the project. `--resources` copies a root containing the input and imports/assets, preserving relative paths. The destination must be outside it. LV2 metadata generation loads the selected plugin during compilation.

## Select a plugin {#selection}

`--plugin-id` accepts a CLAP ID, LV2 URI or a VST3 class ID with 32 hexadecimal characters in native byte order. Without it, the first compatible plugin is selected. Wrapper identity depends on source format, input name and selection rather than the absolute machine path.

## Options and resumed builds {#options}

Use `--name` for the result name, `--backend-root` for installed sources and `--bundle-id` for Apple identity. `--au-sdk` selects AudioUnitSDK. Build overrides are `--cmake`, `--generator`, `--arch` and `--config`. AUv3 requires the Xcode generator on macOS; Windows cannot compile Apple frameworks.

```sh
cyclapbridge-cli build --project "D:/Exports/Gain-VST3"
```

This resumes a generated project. Retain its payload and notices. See [Distribution](doc:distribution) before moving the built artifacts.
