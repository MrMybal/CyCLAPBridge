# CyCLAPBridge — Version notes

[README](README.md) · [Français](VERSION.fr.md)

**Library, Studio and CLI: 0.2.2 · IN DEVELOPMENT**

## Changes in 0.2.2

- CyMCPSDK 0.7.1 adds four selectable profiles for the loopback MCP server. Free uses the address alone and starts external requests automatically.
- Settings without a profile use Free. Existing selections stay in place. Studio keeps the server off at launch.
- The JSON CLI uses the address alone in Free and Local controlled. Token only and Protected require a Bearer token.

## Changes in 0.2.1

- Native Studio chat with provider discovery, streaming, stop, attachments and optional local conversation restore.
- CyMCPSDK 0.7.0 powers the shared Studio, MCP and CLI tools for format discovery, inspection, conversion and job status.
- The Windows ZIP includes Studio, the CLI and the bilingual offline manual. The original conversion commands are available alongside the JSON tools.

## Current implementation

- Separate Studio application package with a root executable and private conversion components. Convert delivers compiled plugins, payloads and notices; the developer SDK remains separate.
- Standalone static C++20 library, CyUI Studio for Windows/Linux/macOS and conversion CLI.
- CLAP/VST3/LV2/JSFX hosting and wrappers, float32 processing, representable events, latency and state.
- Embedded Windows CLAP, VST3 HWND, LV2 WindowsUI and JSFX `@gfx` editors, including CLAP facades.
- AUv2 hosting, Cocoa editor sources and AUv2/AUv3 export projects; Apple validation deferred.
- Offline English/French manual, localized Studio, shared file browser, themes and F1 help.
- Native CMake/CPack presets: Windows ZIP, Linux X11 tar.gz and macOS app bundle. Only Windows is validated.

## Known limitations

- Native target: Windows x64. macOS and Linux builds are unvalidated; macOS testing is deferred by the owner.
- AAX is excluded. JSFX is input-only. AUv3 does not import arbitrary files; exports need macOS, Xcode and signing.
- The common API does not promise float64, SysEx, MIDI 2, per-voice modulation or complete transport translation.
- LV2 UI Atom/patch, Gtk/Qt adaptation, JSFX file-drop and non-Windows VST3/LV2/JSFX editors are outside current integration.
- DAWs supply process supervision, IPC and security isolation. Foreign code retains its own real-time behavior.
- Studio defaults to English and includes French. The Windows x64 application ZIP is available in [pre-release 0.2.2](https://github.com/MrMybal/CyCLAPBridge/releases/tag/v0.2.2); its compatibility scope follows the limitations above.

These notes maintain no itemized confirmed-bug list. Consult the [issue tracker](https://github.com/MrMybal/CyCLAPBridge/issues); repository access may be required. This does not imply an absence of bugs.
