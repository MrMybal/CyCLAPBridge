---
id: studio
title: Create a wrapper
description: Select a source and deliver a compiled plugin in another format.
section: Manual
parent: overview
order: 10
revision: 4
---

```image
{"id":"studio-interface","src":"assets/studio-en.png","alt":"Studio conversion form with source and target format selection","controls":"minimal"}
```

## Select the input {#input}

1. Open Studio and select the source format on the left and target format on the right.
2. Use **File** for a plugin file or JSFX source, or **Bundle** for an LV2/VST3/AU directory.
3. Choose a new output folder and enter a wrapper name. Existing output folders are refused to avoid overwriting data.
4. Keep the application's `Resources` folder next to its executable; the conversion components are included.

Apple conversion requires macOS, and AUv3 needs Xcode. These targets are unavailable in Windows/Linux Studio. Apple validation remains deferred. Consult [Formats](doc:formats) before selecting a direction.

## Include resources {#resources}

The optional resource/import directory must contain the selected input and its associated files. Their relative paths are preserved. This is useful for JSFX imports, images and plugin resources. Choose a destination outside that source directory. Banks, external dependencies and publisher authorizations remain the plugin's responsibility.

## Generate and compile {#build}

Choose **Convert**. Studio prepares its components, runs the conversion in the background and delivers the compiled plugin, payload and notices to the selected output folder. Read the conversion log if it fails. The app is already compiled; plugin conversion still needs CMake 3.27+ and a native C++20 toolchain. Under Windows, install Visual Studio 2022 Desktop development with C++ and a Windows SDK. Common installed CMake locations are detected automatically.

For LV2 output, conversion runs a metadata helper that loads the plugin. This helper is not a security sandbox.

After success, **Open result** opens the artifacts directory. Keep the wrapper, its `CyCLAPBridge-payload` and `CyCLAPBridge-licenses` together. Nothing is automatically installed in a DAW's plugin folders. Follow the DAW's normal discovery procedure when installing the complete result.

## Select a plugin inside a module {#selection}

Studio uses the first compatible plugin in a multi-plugin module by default. Enter its CLAP ID, LV2 URI or VST3 class ID in **Plugin ID (optional)** under **Advanced options** to select a particular plugin. The [CLI](doc:cli#selection) provides the equivalent `--plugin-id` option and additional Apple bundle options.

## Language, theme and tools {#settings}

Choose English or French and a theme at the top. Preferences are saved in the operating system’s per-user configuration directory. The shared file browser works on local filesystem roots; full paths can also be pasted into the fields. Advanced options select a plugin ID and let you override CMake, generator, architecture and configuration when needed.
