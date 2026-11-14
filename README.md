<table width="100%">
  <tr>
    <td width="300" align="left" valign="top">
      <a href="Documentation/Documentation.html#/en/overview">Offline manual</a><br>
      <a href="https://github.com/MrMybal/CyCLAPBridge">Repository</a><br><br>
      <sub>README</sub><br><a href="README.md">English</a> · <a href="README.fr.md">Français</a>
    </td>
    <td width="400" align="center" valign="middle"><strong>CyCLAPBridge</strong><br><sub>C++20 · Studio · CLI</sub></td>
    <td width="300" align="right" valign="top">
      <p><sub>Application license</sub><br><a href="LICENSE"><strong>MIT</strong></a></p>
      <p><sub>Version</sub><br><a href="VERSION.md"><strong>0.2.0</strong></a><br><strong>IN DEVELOPMENT</strong></p>
      <sub>Windows x64 · source build</sub>
    </td>
  </tr>
</table>

<h1 align="center">CyCLAPBridge</h1>

Standalone C++20 library, CLI and native Windows application for hosting audio plugins and creating format wrappers. For plugin users building wrappers and DAW developers integrating hosting. No dependency on CyAudioCore, CyDSP, CyDAW, CyUI or a third-party UI toolkit.

> **IN DEVELOPMENT · 0.2.0** — Windows x64 is the current native target. Apple integrations are in source; macOS builds and tests are deferred. Read the [version notes and limitations](VERSION.md).

## Create wrappers or integrate a host

**Studio** creates a CMake project from a plugin or JSFX script. Choose the source and target formats, input, a new project directory and result name. **Créer le projet** generates sources; **Créer et compiler** also compiles on a background thread. **Ouvrir le résultat** opens the artifacts. Studio currently uses French labels; F1 opens the bilingual manual.

The CLI also inspects modules. An optional resource directory preserves imports and assets. Keep the wrapper, `CyCLAPBridge-payload` and notices together. Conversion adapts an API around the original payload; it does not recompile that plugin or change its redistribution terms.

The static library exposes `cybridge::HostedPlugin` and direct `cybridge::ClapPlugin` hosting. Public headers keep plugin SDKs private. The DAW selects the process and owns workers, IPC, crash recovery and security sandboxing. Instances and buffers remain local to that process. Foreign plugins/scripts retain their own real-time behavior.

## Formats

| Format | Input | Output | Current scope |
| --- | --- | --- | --- |
| CLAP | Yes | Yes | Windows audio, automation, state, editors and wrappers |
| VST3 | Yes | Yes | Windows audio, automation, state and HWND editors |
| LV2 | Yes | Yes | Turtle metadata, audio, MIDI Atom, state and WindowsUI |
| JSFX | EEL2 through ysfx | No standalone native-to-script export | Windows audio, `@gfx`, CLAP/VST3/LV2 exports |
| AUv2 | Apple adapter | clap-wrapper export | Source integration; macOS validation deferred |
| AUv3 | No arbitrary-file import | Extension and container app | Xcode generation; macOS validation deferred |

AAX is excluded. A JSFX script cannot load arbitrary native plugins without a REAPER extension. The common API uses float32 and representable parameter/note/MIDI 1 events; float64, SysEx, MIDI 2, per-voice modulation and full transport translation are outside its current contract.

---

## Installation

No application download is published yet. Build from source using **CMake 3.27+**, a **C++20 compiler**, and, for Windows, **Visual Studio 2022 Desktop development with C++** and a Windows SDK. Generated wrappers also require a C++ toolchain. Plugin folders are not modified automatically.

After installation, launch `dist/CyCLAPBridge/bin/CyCLAPBridgeStudio.exe` or `cyclapbridge-cli.exe` in the same directory. Open `Documentation/Documentation.html` from the checkout, or `share/CyCLAPBridge/Documentation/Documentation.html` from an installation. Keep the whole manual directory. GitHub shows HTML source; read this manual locally after cloning/downloading.

## Sources and build

```sh
git clone https://github.com/MrMybal/CyCLAPBridge.git
cd CyCLAPBridge
git submodule update --init third_party/clap-wrapper third_party/clap third_party/vst3sdk third_party/AudioUnitSDK
git -C third_party/vst3sdk submodule update --init base public.sdk pluginterfaces cmake
cmake -S . -B build/release -G "Visual Studio 17 2022" -A x64
cmake --build build/release --config Release
cmake --install build/release --config Release --prefix dist/CyCLAPBridge
```

Private repository access requires permission. SDKs are pinned; LV2/ysfx runtimes are retained snapshots. Configuration and conversion do not download dependencies. The default installation supplies the offline conversion sources.

<details>
<summary>Static library integration and optional targets</summary>

```cmake
find_package(CyCLAPBridge 0.2 CONFIG REQUIRED)
target_link_libraries(MyDaw PRIVATE CyCLAPBridge::CyCLAPBridge)
```

Select an installation with `CMAKE_PREFIX_PATH`. Include `<cyclapbridge/Plugin.h>` or `<cyclapbridge/Conversion.h>`; see the [DAW example](https://github.com/MrMybal/CyCLAPBridge/blob/main/examples/daw/main.cpp) and [library manual](Documentation/Documentation.html#/en/library).

Set `CYCLAPBRIDGE_BUILD_CLI=OFF`, `CYCLAPBRIDGE_BUILD_GUI=OFF` and `CYCLAPBRIDGE_INSTALL_BACKEND=OFF` for a library-only installation retaining headers, documentation and notices. Tests are opt-in through `CYCLAPBRIDGE_BUILD_TESTS=ON`.

Studio is Windows-only. AUv2 needs macOS and AudioUnitSDK; AUv3 also needs Xcode and signing. Linux/Apple target validation is outstanding. The included manual needs no Node.js; regenerating it separately requires Node.js 22+ and the [.CyDocBuild instructions](https://github.com/MrMybal/CyCLAPBridge/blob/main/.CyDocBuild/README.md).

</details>

## CLI examples

```sh
cyclapbridge-cli capabilities
cyclapbridge-cli inspect --from jsfx --input "D:/Effects/Gain.jsfx"
cyclapbridge-cli convert --from clap --to lv2 --input "D:/Plugins/Gain.clap" --output "D:/Exports/Gain-LV2" --build
cyclapbridge-cli convert --from jsfx --to vst3 --input "D:/Effects/Gain.jsfx" --resources "D:/Effects" --output "D:/Exports/Gain-VST3" --build
cyclapbridge-cli build --project "D:/Exports/Gain-VST3"
```

`--plugin-id` selects a CLAP ID, LV2 URI or VST3 class ID. Inspection executes plugin code. LV2 export loads the plugin in a metadata helper during compilation. See the [CLI manual](Documentation/Documentation.html#/en/cli).

---

## License

Original code uses [MIT](LICENSE). The pinned **VST3 SDK 3.8.0 build 66 is MIT**; VST3 conversion does not impose GPL on CyCLAPBridge. Payloads retain their own licenses. Keep full notices; see [distribution terms](docs/Licensing.md) and [pinned provenance](THIRD_PARTY.md).

| Library · source | License · terms | Use / scope |
| --- | --- | --- |
| [CLAP](https://github.com/free-audio/clap), [clap-wrapper](https://github.com/free-audio/clap-wrapper) | [CLAP MIT](https://github.com/free-audio/clap/blob/195b42a004144fab0b3cf95e9c067187d15365b7/LICENSE), [wrapper MIT](https://github.com/free-audio/clap-wrapper/blob/806bba0a5f792e3966b99c1561ab9952d3316d7c/LICENSE) | Host ABI and conversion backend |
| [VST3 SDK](https://github.com/steinbergmedia/vst3sdk) | [MIT](https://github.com/steinbergmedia/vst3sdk/blob/9fad9770f2ae8542ab1a548a68c1ad1ac690abe0/LICENSE.txt), nested notices | VST3 hosting/conversion |
| [AudioUnitSDK](https://github.com/apple/AudioUnitSDK) | [Apache-2.0](https://github.com/apple/AudioUnitSDK/blob/53a9a2008aae7fb1b0a9f093dd523b9b12f6c0d9/LICENSE.txt) | Apple conversion |
| [LV2](https://github.com/lv2/lv2), [Lilv](https://github.com/lv2/lilv), [Serd](https://github.com/drobilla/serd), [Sord](https://github.com/drobilla/sord), [Sratom](https://github.com/lv2/sratom), [Zix](https://github.com/drobilla/zix) | [ISC notices](third_party/licenses/) | Static discovery, metadata and LV2 hosting |
| [ysfx](https://github.com/jpcima/ysfx), WDL/LICE and embedded components | [Apache-2.0, Zlib, MIT/ISC and component terms](third_party/licenses/ysfx.txt); parser GPL-2.0-or-later **with Bison-exception-2.2** | JSFX audio/Windows `@gfx`; parser exception permits a separate larger-work license |
| fmt and PreSonus declarations in clap-wrapper | [MIT and public-domain notices](THIRD_PARTY.md) | Wrapper formatting/interfaces |
| [CyDoc](https://github.com/MrMybal/CyDoc) | [MIT](Documentation/DocData/licenses/CyDoc.txt) | Separate offline manual; no C++ runtime dependency |
| [marked](https://github.com/markedjs/marked), [PrismJS](https://github.com/PrismJS/prism), [esbuild](https://github.com/evanw/esbuild) | [MIT notices](Documentation/DocData/licenses/) | Documentation generation only |

## References

- [English manual](Documentation/Documentation.html#/en/overview) · [Manuel français](Documentation/Documentation.html#/fr/overview)
- [Repository](https://github.com/MrMybal/CyCLAPBridge) · [Issues](https://github.com/MrMybal/CyCLAPBridge/issues) · [Version notes](VERSION.md)
- [Editor integration](docs/Editors.md) · [Architecture](docs/Architecture.md) · [Steinberg licensing FAQ](https://steinbergmedia.github.io/vst3_dev_portal/pages/FAQ/Licensing.html)
