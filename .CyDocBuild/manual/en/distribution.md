---
id: distribution
title: Distribute a wrapper or library
description: Keep payloads, required resources and component notices together.
section: Manual
parent: overview
order: 60
revision: 1
---

## Keep the complete result {#files}

Keep the wrapper with its `CyCLAPBridge-payload`, resources and `CyCLAPBridge-licenses`. Some bundle formats place these inside the bundle. Moving only the outer DLL/plugin can break loading. External banks, runtimes and publisher permissions remain applicable. CyCLAPBridge does not automatically install plugins.

The installed static library includes notices in `share/CyCLAPBridge/licenses`, even with the source backend disabled. Distribute those notices with binaries incorporating the library. Keep the complete `Documentation` export when shipping the manual.

## Licenses {#licenses}

Original CyCLAPBridge code is MIT. The pinned VST3 SDK 3.8.0 build 66 and its nested components are MIT; VST3 support does not impose GPL on this project. AudioUnitSDK and ysfx are Apache-2.0. LV2, Lilv, Serd, Sord, Sratom and Zix retain ISC notices. Embedded WDL/LICE, image/audio helpers and other components retain their separate terms.

The generated EEL2 parser retains GPL-2.0-or-later with Bison-exception-2.2. This exception permits a larger work containing the parser skeleton to use separate terms in this application. Preserve the parser's notice. The root README license table and `THIRD_PARTY.md` index the retained full notices and exact revisions.

The separate manual reader uses CyDoc under MIT. Its notices, and those for marked, PrismJS and esbuild used during generation, are in `DocData/licenses`.

## Selected plugin rights {#payload}

Wrapping does not grant redistribution rights or replace the selected plugin's license. A GPL payload may add distribution obligations; a proprietary payload retains its restrictions. Keep applicable copyright, attribution and license texts for every included resource or script.
