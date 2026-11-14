# CyCLAPBridge — Version notes

[README](README.md) · [Français](VERSION.fr.md)

**Library, Studio and CLI: 0.2.0 · IN DEVELOPMENT**

## Current implementation

- Standalone static C++20 library, native Windows Studio and conversion CLI.
- CLAP/VST3/LV2/JSFX hosting and wrappers, float32 processing, representable events, latency and state.
- Embedded Windows CLAP, VST3 HWND, LV2 WindowsUI and JSFX `@gfx` editors, including CLAP facades.
- AUv2 hosting, Cocoa editor sources and AUv2/AUv3 export projects; Apple validation deferred.
- Offline English/French manual and Studio help access.

## Known limitations

- Native target: Windows x64. macOS and Linux builds are unvalidated; macOS testing is deferred by the owner.
- AAX is excluded. JSFX is input-only. AUv3 does not import arbitrary files; exports need macOS, Xcode and signing.
- The common API does not promise float64, SysEx, MIDI 2, per-voice modulation or complete transport translation.
- LV2 UI Atom/patch, Gtk/Qt adaptation, JSFX file-drop and non-Windows VST3/LV2/JSFX editors are outside current integration.
- DAWs supply process supervision, IPC and security isolation. Foreign code retains its own real-time behavior.
- Studio uses French labels. No binary release or production compatibility list is published.

These notes maintain no itemized confirmed-bug list. Consult the [issue tracker](https://github.com/MrMybal/CyCLAPBridge/issues); repository access may be required. This does not imply an absence of bugs.
