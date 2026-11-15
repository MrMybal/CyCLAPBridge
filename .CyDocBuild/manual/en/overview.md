---
id: overview
title: CyCLAPBridge
description: Choose between creating a wrapper and integrating plugin hosting.
section: Manual
order: 0
revision: 2
---

## Start here {#getting-started}

CyCLAPBridge 0.2.0 is in development. It combines a static C++20 hosting library, a conversion CLI and a CyUI desktop Studio. It is independent of other Cy projects and of third-party UI toolkits.

For a plugin or script you want to use in another format, follow [Create a wrapper](doc:studio). For a DAW implementation, start with [Integrate the library](doc:library). The [format matrix](doc:formats) explains the supported directions and their boundaries.

## Open the tools {#tools}

After building and installing the sources, run `bin/CyCLAPBridgeStudio.exe` or `bin/cyclapbridge-cli.exe` in the installation. Studio defaults to English and offers a French language choice, built-in themes and a shared file browser. Press F1 or choose its help button to open this manual. Use the language selector for the complete French edition.

The manual opens directly from `Documentation.html`; keep its `DocData` directory beside it. Search from the header or with Ctrl+K. The navigation tree groups tasks and reference pages. No Node.js runtime or web server is required for reading.

## Understand the wrapper {#wrapper}

A wrapper adapts an API around an existing plugin binary or JSFX source. It keeps the original payload and its resources; it does not translate machine code or alter the payload's license. Keep the generated artifacts and notices together. See [Distribution](doc:distribution).

## Hosting process {#process}

The DAW chooses the process, creates and supervises workers, transports audio/events through IPC, recovers after crashes and provides any security sandbox. CyCLAPBridge runs inside the chosen process; it does not create this isolation. Its C++ instances, handles and audio spans stay local to that process. Foreign plugins and scripts keep their own real-time behavior.

## Platform applications {#platforms}

On Windows, run `bin/CyCLAPBridgeStudio.exe`; on Linux, `bin/CyCLAPBridgeStudio`. The prepared macOS installation contains `CyCLAPBridgeStudio.app`. Native Linux/macOS builds still require validation. Keep the complete installed tree. The Mac app stores its manual and conversion resources inside `Contents/Resources/CyCLAPBridge`.
