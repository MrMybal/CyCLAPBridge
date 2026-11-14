# Native validation

Validated on 2026-11-14 on Windows x64 with MSVC 19.40 (Visual Studio 2022)
and Windows SDK 10.0.26100.0, CMake 3.31.6, Release configuration.
Editor acceptance was extended on 2026-11-15 with the same toolchain.

- Library, CLI and native Studio compile.
- cybridge-native: shared module ownership, lifecycle, float32 stereo,
  sample-offset automation, output events, invalid input preserving output,
  output capacity reporting, bounded state and truncated state rejection.
- A C++ new/delete audit surrounds prepared host processing in the test executable.
  It does not intercept every allocator or code path of arbitrary foreign DLLs.
- Conversion generation copies the fixture, refuses existing destinations,
  and rejects unsupported targets and unsafe names.
- The AAX removal regression tests pass. The public format parser and
  CLI reject AAX and its former SDK option, capabilities omit it, and Studio
  exposes five target formats. The multiformat tests also check retained runtime
  and nested VST3 license notices in the generated distributable.
- cybridge-studio-smoke creates the native window and verifies its controls.
  It is a creation smoke test, not full interactive or visual acceptance.
- cybridge-conversion-e2e creates and compiles a VST3, relocates the artifact
  package, then loads and processes that VST3 in an original minimal test host.
  It verifies local CLAP discovery, stereo audio, automation at sample 17 and
  clean module unloading. Required upstream notices accompany the artifact.
- CMake install and an external find_package consumer build and run successfully.
- The library-only build with CLI, GUI, tests and backend installation disabled
  also builds and installs. It includes 17 byte-identical upstream/original
  notices, no backend sources or application executables. An external consumer
  links only the installed package and processes a prepared JSFX fixture block.
  Both full and library-only notice sets were SHA-256 compared to their sources.
- An external GUI acceptance consumer builds with find_package against the
  library-only installation using public headers, without plugin SDK headers.
  It exercises native JSFX @gfx directly and through a CLAP wrapper built by the
  installed CLI using its packaged source SDK, including input, pixels, audio,
  editor lifecycle and state restoration.
- 0.2.0 adds direct JSFX, VST3 and LV2 hosts and native-to-CLAP facades.
  Multi-format acceptance builds CLAP->LV2, LV2->CLAP, JSFX->CLAP,
  JSFX->VST3, VST3->CLAP and JSFX->LV2 and processes their audio.
- AUv2 hosting/export and AUv3 extension/container generation are source
  integrations on this Windows machine. macOS compilation, signing and loading
  require native acceptance. Native-to-JSFX script export is explicitly
  unsupported.
- Windows editor tests create native CLAP, LV2 WindowsUI and JSFX @gfx editors
  using original fixtures. They check repeated open/close, child widget cleanup,
  editor opening before activation, UI-to-audio parameter edits, host automation,
  resizing, rejected wrong-thread destruction and stopped-editor state restoration.
  Audio also runs on a separate thread while the UI is pumped. The prepared
  C++ new/delete audit covers this concurrent path in the test executable.
- JSFX graphics acceptance renders the original @gfx fixture into a GDI bitmap
  and checks its BGRA colour; it does not require a visible desktop window.
- GUI conversion acceptance builds and exercises CLAP->VST3, VST3->CLAP,
  LV2->CLAP, JSFX->CLAP, LV2->VST3 and JSFX->VST3. These include asynchronous
  GUI automation through nested wrappers and restoration before first activation.
  The Windows suite now contains nine passing test cases, including the earlier
  audio conversion suites and the new GUI acceptance.
- The AU Cocoa factory/NSView adapter, parameter listener and resize observer are
  Apple-only sources. Per the owner's request, native macOS testing is deferred;
  this source integration is not a claim of successful Apple compilation/loading.
- The final state acceptance also restores each of the six conversion outputs
  before its first activation, then processes a prepared block and checks the
  restored gain. The installed 0.2.0 CLI compiles JSFX->CLAP using its packaged
  source SDK; an external find_package consumer hosts the JSFX fixture through
  the installed format-neutral library.
- All 415 upstream runtime files listed in the retained provenance manifest
  were SHA-256 checked locally and matched. The manifest was preserved intact;
  the build-selection note in THIRD_PARTY.md distinguishes historical adapters
  from the original ysfx files compiled here.
- The installed CLI uses its packaged backend to convert a fixture with accented
  input/output paths and a Unicode payload filename. The resulting VST3 passes
  the audio acceptance on this system. The Windows linker warns about non-ASCII
  DLL output names on other ANSI code pages; use ASCII wrapper names for wider
  binary distribution until that case is validated.

Tests use an original gain plugin, no physical device and no installed third-party
plugin. No plugin installation, publishing, CI or antivirus settings are involved.
Linux, macOS, ARM64, production DAWs and commercial plugin compatibility are
separate validation work. See the capability table in README.fr.md.
