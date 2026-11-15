---
id: studio
title: Create a wrapper
description: Select a source, preserve resources and build a new conversion project.
section: Manual
parent: overview
order: 10
revision: 2
---

```image
{"id":"studio-interface","src":"assets/studio-en.png","alt":"Studio conversion form with source and target format selection","controls":"minimal"}
```

## Select the input {#input}

1. Open Studio and select the source format on the left and target format on the right.
2. Use **File** for a plugin file or JSFX source, or **Bundle** for an LV2/VST3/AU directory.
3. Choose a new project directory and enter a wrapper name. Existing project directories are refused to avoid overwriting data.
4. Keep the detected CyCLAPBridge component directory, or choose the installed directory containing `third_party` and `templates`.

Studio can generate Apple projects; building them needs macOS, and AUv3 needs Xcode. On Windows/Linux, use Create project for these targets; Create and build is unavailable. Consult [Formats](doc:formats) before selecting a direction.

## Include resources {#resources}

The optional resource/import directory must contain the selected input and its associated files. Their relative paths are preserved. This is useful for JSFX imports, images and plugin resources. Choose a destination outside that source directory. Banks, external dependencies and publisher authorizations remain the plugin's responsibility.

## Generate and compile {#build}

**Create project** writes a CMake project without opening the input plugin. **Create and build** also runs CMake and the compiler on a background thread; read the journal if it fails. For LV2 output, compilation runs a separate metadata helper that loads the plugin. This helper is not a security sandbox.

After success, **Open result** opens the artifacts directory. Keep the wrapper, its `CyCLAPBridge-payload` and `CyCLAPBridge-licenses` together. Nothing is automatically installed in a DAW's plugin folders. Follow the DAW's normal discovery procedure when installing the complete result.

## Select a plugin inside a module {#selection}

Studio uses the first compatible plugin in a multi-plugin module. Use the [CLI](doc:cli#selection) with `--plugin-id` when a specific ID or URI is needed. The CLI also exposes custom CMake generators, architecture, configuration and Apple bundle options.

## Language, theme and tools {#settings}

Choose English or French and a theme at the top. Preferences are saved in the operating system’s per-user configuration directory. The shared file browser works on local filesystem roots; full paths can also be pasted into the fields. Advanced options select component sources, a plugin ID, the CMake executable, generator, architecture and configuration. CMake must be installed; common Windows/macOS locations are detected and a custom executable can be selected.
