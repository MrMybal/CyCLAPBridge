---
id: library
title: Integrate the library
description: Link the static C++20 package and own the hosting lifecycle in a DAW.
section: Developer reference
order: 40
revision: 1
---

## Link the installed package {#link}

```cmake
find_package(CyCLAPBridge 0.2 CONFIG REQUIRED)
target_link_libraries(MyDaw PRIVATE CyCLAPBridge::CyCLAPBridge)
```

Select the installation prefix through `CMAKE_PREFIX_PATH`. Public headers are `cyclapbridge/Conversion.h`, `cyclapbridge/Plugin.h` and `cyclapbridge/Editor.h`; namespace `cybridge`. The package is static and keeps audio SDK headers private. It imports no audio device or GUI toolkit.

## Lifecycle and threads {#lifecycle}

Construct `HostedPlugin(Format, absolutePath, optionalPluginId)` on the creator/UI thread inside the DAW-selected process. Read `descriptor()`, `inputs()`, `outputs()` and `parameters()` before preparing. Allocate the DAW's planar buffers and event storage before starting audio.

Call `prepare(sampleRate, maximumFrames, maximumEvents)` on the control thread. One serialized audio owner calls `start()`, repeated `process(ProcessBlock&)`, then `stop()`. After audio stops, control code can call `release()`, change configuration or destroy the instance. Do not overlap processing with lifecycle or state operations.

`ProcessBlock` contains frame count, bus spans, input events and caller-owned output-event capacity. Check `ProcessStatus`, `outputEventCount` and `outputEventsDropped`. Its pointers/spans must remain valid for the call and local to the hosting process. Prepared adapter processing uses bounded storage; arbitrary foreign DLL/script code does not inherit this real-time guarantee.

## Requests and state {#state}

Call `pumpMainThread()` regularly on the creator thread and handle returned request flags. Restart/rescan requires a stopped/deactivated lifecycle as appropriate. For activated CLAP instances, `flushRequested` and `processRequested` ask the DAW to schedule audio work; the UI pump does not replace the audio owner.

`latencyFrames()` reports latency. `saveState(maximumBytes)` returns opaque bounded bytes; `loadState(span)` restores them with audio stopped. `HostedPlugin` includes pending UI values in its state envelope. Store that state as opaque data rather than interpreting its layout. `parameterValue(id, value)` is a creator/UI-thread query.

## Conversion API {#conversion}

`backendFromRoot(path)` selects the retained conversion sources. Fill a `ConversionRequest`, then call `createConversionProject(request)` on a control/background thread. It returns `ConversionProject` and throws on invalid input, unsupported directions or I/O failure. It refuses existing destinations.

`buildConversionProject(project, options, logSink)` runs CMake directly without a command shell and returns `BuildResult`. Neither generation nor compilation belongs on the audio thread. The DAW supplies any process supervision or security boundary. See [Command line](doc:cli) for corresponding options and [Editors](doc:editors) for GUI ownership.
