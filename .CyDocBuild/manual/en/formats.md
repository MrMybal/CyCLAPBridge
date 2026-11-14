---
id: formats
title: Formats and limitations
description: Understand which input/output directions and native editors are represented.
section: Manual
parent: overview
order: 30
revision: 1
---

## Conversion matrix {#matrix}

| Format | Input | Output | Platform boundary |
| --- | --- | --- | --- |
| CLAP | Yes | Yes | Direct hosting and generated facades |
| VST3 | Yes | Yes | Pinned MIT SDK; Windows hosting/editor path |
| LV2 | Yes | Yes | Bundle metadata, audio, MIDI Atom and state |
| JSFX | Yes, EEL2 source | No standalone script target | ysfx runtime; Windows `@gfx` |
| AUv2 | Apple host adapter | clap-wrapper | macOS required; source integration unvalidated |
| AUv3 | No arbitrary-file input | Extension and container | macOS/Xcode/signing required; unvalidated |

Apple implementation is available in source, with native testing deferred because the owner has no Mac. Linux builds remain unvalidated. Studio itself is Windows-only. AAX has no input, output or SDK option.

## JSFX {#jsfx}

JSFX is a script language. A normal JSFX script does not load native CLAP/VST3/LV2/AU plugins. Such an output would need a native REAPER extension; CyCLAPBridge rejects it. The pinned ysfx runtime handles up to 64 sliders and 64 channels. Preserve imports and resources. REAPER-specific functions may require facilities outside a standalone host.

## Shared audio and editor contract {#common}

The shared API represents float32 buses, parameter events, notes and MIDI 1 messages. VST3 values are normalized to [0,1]; other backends retain native units. Unrepresentable events are rejected or reported through `outputEventsDropped`. Float64, SysEx, MIDI 2, per-voice modulation and full transport translation are outside this contract.

Embedded editors cover CLAP's native GUI, VST3 HWND, LV2 WindowsUI and JSFX `@gfx` on Windows. AU Cocoa is an unvalidated source integration. LV2 Gtk/Qt adaptation, UI Atom/patch, JSFX file-drop and non-Windows VST3/LV2/JSFX editors are outside current scope. See [Editors](doc:editors).
