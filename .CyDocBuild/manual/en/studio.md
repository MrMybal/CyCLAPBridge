---
id: studio
title: Create a wrapper
description: Select a source and deliver a compiled plugin in another format.
section: Manual
parent: overview
order: 10
revision: 7
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

## Assistants in Studio {#assistants}

1. Open **Chat** and expand **Providers**. Select **Demo / Markdown (no AI)**, then **Connect / New chat** to try the panel locally.
2. For an AI conversation, choose **Detect providers** to find installed CLIs, configured API credentials and running local endpoints, or select a provider manually. Select the connection and connect. Studio streams the reply and **Stop** cancels the current reply. A native CLI provider uses the installed, signed-in CLI. An API provider uses the key from the named environment variable, if required. For an installed local model service, use its endpoint. If Studio offers a model list, choose a model and connect again.
3. For a provider that supports tools, keep **Give this chat access to Studio tools** selected when connecting. Ask for an inspection or conversion and supply the relevant paths. Review the request in Studio and choose **Run request** or **Decline**. Only approve plugin files you trust, since inspection and some conversions run plugin code.

For an external assistant on the same machine, expand **External assistants (MCP)** and choose **Start local MCP server**. **Copy MCP connection** gives you the HTTP URL and Bearer header for an MCP client. Paste the value into that client's server configuration. Keep the token private. **Stop local MCP server** revokes it. Starting the server again gives you a new connection.

Turn on **Save chat history locally** before connecting to save each turn under a new history ID. To resume, select the same provider, enter that ID and choose **Restore saved chat**.

| Tool | What the assistant can do |
| --- | --- |
| `list_formats` | Read the available inspection formats and conversion directions on this platform. |
| `inspect_plugin` | Request metadata and audio bus/parameter counts for a plugin file, bundle or JSFX script. |
| `convert_plugin` | Request a compiled wrapper. Supply the source and target formats, an absolute input path and a new absolute output folder. Optional fields select a plugin ID, wrapper name, resource folder or Apple bundle ID. |
| `get_job` | Check a request by ID and read its result, error or build log. |

Inspection and conversion return a job ID immediately. A completed conversion writes the same wrapper, payload and notices as **Convert** in Studio. The provider and MCP connection stay in memory during the Studio session.

### Scope of the MCP tools {#assistant-scope}

Use **Run request** to approve the exact input, destination and options shown in Studio. **Decline** leaves the input and destination untouched. A running conversion uses the regular Studio build workflow. Conversion jobs have no cancel or resume operation, and the eight most recent assistant job results are kept in memory for the Studio session.

File selection, opening the result in the system file browser and appearance settings are local interface actions. The tools take explicit paths and return the output folder so clients can use it directly. Studio accepts incoming MCP calls for its own audio functions. It does not import tool catalogues from other apps because inspection and conversion are self-contained.

The four Studio tools are sent as a complete catalogue so assistants can call them directly. MCP clients may cache their schemas for 30 seconds. Job status is fetched on every call. Studio keeps the full conversation history and does not set a model context window. Its bounded conversion logs and fixed tool catalogue need no resource store, filesystem search, change subscriptions or performance log.
