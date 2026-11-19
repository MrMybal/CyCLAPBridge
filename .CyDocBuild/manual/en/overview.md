---
id: overview
title: CyCLAPBridge
description: Choose between creating a wrapper and integrating plugin hosting.
section: Manual
order: 0
revision: 3
---

## Start here {#getting-started}

CyCLAPBridge 0.2.1 is in development. It combines a static C++20 hosting library, a conversion CLI and a CyUI desktop Studio. The audio library is independent of other Cy projects and UI toolkits; the optional Studio uses CyUI.

For a plugin or script you want to use in another format, follow [Create a wrapper](doc:studio). For a DAW implementation, start with [Integrate the library](doc:library). The [format matrix](doc:formats) explains the supported directions and their boundaries.

## Open the tools {#tools}

Extract the Studio application package and run `CyCLAPBridgeStudio.exe` directly at its root on Windows. Keep `Resources` next to it. Studio defaults to English and includes French, themes and a shared file browser. F1 opens this manual. Plugin conversion needs CMake 3.27+ and a native C++20 toolchain; the application itself is already compiled.

The manual opens directly from `Documentation.html`; keep its `DocData` directory beside it. Search from the header or with Ctrl+K. The navigation tree groups tasks and reference pages. No Node.js runtime or web server is required for reading.

## Understand the wrapper {#wrapper}

A wrapper adapts an API around an existing plugin binary or JSFX source. It keeps the original payload and its resources; it does not translate machine code or alter the payload's license. Keep the generated artifacts and notices together. See [Distribution](doc:distribution).

## Hosting process {#process}

The DAW chooses the process, creates and supervises workers, transports audio/events through IPC, recovers after crashes and provides any security sandbox. CyCLAPBridge runs inside the chosen process; it does not create this isolation. Its C++ instances, handles and audio spans stay local to that process. Foreign plugins and scripts keep their own real-time behavior.

## Platform applications {#platforms}

On Windows, run `CyCLAPBridgeStudio.exe`; on Linux, `CyCLAPBridgeStudio`. The prepared macOS application package contains `CyCLAPBridgeStudio.app`. Native Linux/macOS builds still require validation. Keep the complete application folder. The Mac app stores its manual and conversion components in `Contents/Resources/CyCLAPBridge`. The application package includes the CLI when built. The developer SDK is distributed separately.
