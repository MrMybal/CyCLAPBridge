---
id: editors
title: Embed a plugin editor
description: Own native parent windows, pump UI work and forward automation safely.
section: Developer reference
parent: library
order: 50
revision: 1
---

## Native parent ownership {#parent}

The DAW owns the parent window. Editor calls and `pumpMainThread()` run on the instance's creator/UI thread; on macOS this must also be Cocoa's main thread. No SDK or GUI framework is required in public headers.

Call `editorSupported(api)`, then `createEditor(api, callbacks)`. Query `editorSize()`, size the parent and attach it with `setEditorParent({api, nativeParent})`. Show it with `showEditor(true)`. If attachment fails, destroy the editor. Call `destroyEditor()` before destroying its parent or plugin.

`EditorCallbacks` holds a context pointer and noexcept callbacks for resize requests, parameter gestures and closure. Keep the context alive until editor destruction returns. A resize callback returns false when the DAW rejects a size. Optional methods expose resizing constraints, adjusted sizes and scale support; do not assume every plugin supports them.

## Pump and automation {#automation}

Pump UI work about every 33 ms while open. This services CLAP timers, LV2 idle, JSFX graphics and parameter synchronization. Editor open/close can coexist with audio; preparation changes and save/restore remain stopped-audio control operations.

UI parameter values are coalesced per parameter and merged at the start of the next audio block. A DAW event at frame zero wins over a pending UI value in the same block. CLAP output gestures use `EventKind::gestureBegin` and `gestureEnd`; they are not common-API audio inputs.

The generated CLAP facade needs host `CLAP_EXT_TIMER_SUPPORT` for its editor. Its bounded gesture queue has 1023 usable entries; overflow requests a value rescan. Audio values remain coalesced. The DAW remains responsible for scheduling requested audio processing.

## Backend boundaries {#backends}

- CLAP forwards a plugin-provided compatible embedded GUI.
- VST3 uses `IPlugView`/`IPlugFrame` on HWND; DPI support depends on the plugin.
- LV2 supports WindowsUI, scalar float controls and supported host features, not Gtk/Qt adaptation or UI Atom/patch.
- JSFX scripts with `@gfx` use ysfx/WDL/LICE on Windows, including mouse, keyboard and menus. Scripts without `@gfx` have no native editor; a DAW may display sliders. File-drop is unavailable.
- AU Cocoa editor sources are integrated but unvalidated on macOS. Non-Windows VST3/LV2/JSFX editors are not advertised.

Plugins and scripts keep their own GUI dependencies, allocations, synchronization and file access. See [Formats](doc:formats) and [Process ownership](doc:overview#process).
