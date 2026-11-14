# Pinned upstream sources

CyCLAPBridge's original C++ sources are MIT. No CyAudioCore, CyDSP, CyDAW,
CyUI or third-party UI toolkit is required.

| Component | Repository | Revision | License |
| --- | --- | --- | --- |
| clap-wrapper | https://github.com/free-audio/clap-wrapper | 806bba0a5f792e3966b99c1561ab9952d3316d7c | MIT |
| CLAP 1.2.10 | https://github.com/free-audio/clap | 195b42a004144fab0b3cf95e9c067187d15365b7 | MIT |
| VST3 SDK 3.8.0 build 66 | https://github.com/steinbergmedia/vst3sdk | 9fad9770f2ae8542ab1a548a68c1ad1ac690abe0 | MIT |
| VST3 base | https://github.com/steinbergmedia/vst3_base | 3d2e82f8e6bff59c1d8b7a27491a29c2286b5206 | MIT |
| VST3 pluginterfaces | https://github.com/steinbergmedia/vst3_pluginterfaces | 31d6eeba6daaa3e2a8bfbe3e7a90ca0b7fbfbc1c | MIT |
| VST3 public.sdk | https://github.com/steinbergmedia/vst3_public_sdk | a3911a4615dabbfdfd9d181ee26b05c70c289a95 | MIT |
| VST3 CMake | https://github.com/steinbergmedia/vst3_cmake | de6e54eeaaab35b7145f5c32c279b5e892146e04 | MIT |
| AudioUnitSDK 1.1.0 | https://github.com/apple/AudioUnitSDK | 53a9a2008aae7fb1b0a9f093dd523b9b12f6c0d9 | Apache-2.0 |
| LV2 | https://github.com/lv2/lv2 | 3c57dae600a5ad8d05acd53ee3490f7d91cb7be6 | ISC |
| Lilv | https://github.com/lv2/lilv | 4b8f30055fa5cb4134a2eb45fc1555bc04289314 | ISC |
| Serd | https://github.com/drobilla/serd | b621b0bc790d1e8102d3138ead16b974ddda8432 | ISC |
| Sord | https://github.com/drobilla/sord | 4b5232bb91e7f2f1b84df384494d073fabbcd312 | ISC |
| Sratom | https://github.com/lv2/sratom | 9e8ee84eed502e6030ce8bb49694fd28542b08fc | ISC |
| Zix | https://github.com/drobilla/zix | 17d632393f5addb6fa24dda2927b96515407cca7 | ISC |
| ysfx | https://github.com/jpcima/ysfx | 8077347ccf4115567aed81400281dca57acbb0cc | Apache-2.0 and embedded dependency notices |

Upstream source trees are retained without modification, including all original
copyright and license notices. clap-wrapper also vendors fmt (MIT) and PreSonus
gain-reduction interface declarations (public domain); retain their embedded
notices when redistributing. VST3 name/logo use has its own upstream guidelines.

The LV2/ysfx snapshots were copied from the existing local vendored snapshot,
including its original manifest, exact upstream files and notices. Historical
scope/adaptedFiles entries in third_party/runtime-provenance.json are preserved.
The three historical src/detail/Jsfx* adaptation paths listed there are NOT
compiled or copied into this project: cmake/Runtimes.cmake selects the original
third_party/ysfx/sources/ysfx.cpp and ysfx_api_eel.cpp. This is a build-selection
note, not a modification or replacement of the original provenance record.
ysfx embeds WDL (Zlib), dr_libs/stb and generated parser code with its Bison
exception; see the complete retained third_party/licenses/ysfx.txt notice.
Windows builds also compile the original pinned WDL/LICE and lice_stb sources
already present in this snapshot to enable JSFX @gfx. Their notices remain
unchanged. GUI hosting uses native platform APIs and no third-party UI toolkit.

ASIO, RtAudio, RtMidi and VSTGUI are not required. The AUv3 container uses
the upstream Apple platform integration. Apple code is not Windows runtime code.
Generated projects explicitly disable dependency downloads and plugin installation.

AAX is excluded from CyCLAPBridge. No AAX SDK is vendored, linked or installed.
The unmodified MIT clap-wrapper snapshot retains its upstream optional adapter
sources; generated projects explicitly disable CLAP_WRAPPER_BUILD_AAX and never
call its AAX target helper. The temporary AAX validation SDK/output were removed.

The pinned VST3 SDK and the nested VST3 components above are MIT; VST3 support
does not impose GPL on CyCLAPBridge. Exact notices accompany generated wrappers.
See docs/Licensing.md for the distinction between original code, dependencies
and the selected plugin payload.

templates/PayloadEntry.cpp is original CyCLAPBridge code: a linked CLAP entry
delegates to a payload beside the wrapper. It does not patch clap-wrapper.
The original plugin binary and its redistribution terms belong to its publisher.

The separate HTML manual is generated with CyDoc 0.1.0 (MIT) from the retained
package in .CyDocBuild/vendor. Its package SHA-256 and source revision are recorded
in .CyDocBuild/README.md; npm dependencies are pinned by its package-lock.json.
The reader is distributed with the exact CyDoc notice in Documentation/DocData/licenses.
Generation uses marked (MIT), PrismJS (MIT) and esbuild (MIT), whose notices are
also retained there. Node.js 22+ is a generation prerequisite only. No JavaScript
runtime or documentation generator is linked into the C++ library or Studio.
