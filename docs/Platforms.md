# Studio platform builds

Studio uses one C++20 interface with CyUI Core/FilePicker, one pinned Dear ImGui
target, GLFW 3.4 and the OpenGL 3 renderer. These dependencies belong only to the
optional Studio executable. The static audio library and its public headers do
not depend on them. Configuration never downloads a dependency.

| Target | Preset | Package | Validation |
| --- | --- | --- | --- |
| Windows x64 | `windows-x64` | ZIP | Compiled and exercised on Windows |
| Linux, native architecture | `linux` | tar.gz | Sources/configuration prepared; native validation outstanding |
| macOS 11+, native architecture | `macos` | ZIP containing `.app` | Sources/configuration prepared; native testing deferred |

Do not rename a Windows executable as a Linux/macOS build. Run each preset on
its native OS; there is no universal binary or cross-compilation claim here.

## Source initialization

```sh
git submodule update --init third_party/clap-wrapper third_party/clap third_party/vst3sdk third_party/AudioUnitSDK third_party/CyUI third_party/glfw third_party/imgui
git -C third_party/vst3sdk submodule update --init base public.sdk pluginterfaces cmake
```

CyUI may require repository access. The exact revisions and retained licenses
are in `THIRD_PARTY.md`. Studio supplies its own pinned ImGui target; CyUI's
bundled ImGui copy is not compiled. No changes to CyUI are needed.

## Windows

Install Visual Studio 2022 Desktop development with C++, a Windows SDK and CMake
3.27+. Put CMake on PATH for Studio/CLI conversion builds, or select its executable
in Studio's Advanced options. A working OpenGL 3.0 driver and the matching Visual
C++ runtime are required. No GLFW/CyUI DLL is distributed: those targets are static.
The Windows distribution presets additionally use LLVM object tools, available
with Visual Studio's C++ Clang tools, to omit debug metadata from the installed
static archive. This is build tooling only. Local development installations can
set `CYCLAPBRIDGE_CLEAN_ARCHIVE=OFF` without changing the audio library's API.

```sh
cmake --preset windows-x64
cmake --build --preset windows-x64 --parallel
cmake --install build/windows-x64 --config Release --prefix dist/CyCLAPBridge
cpack --preset windows-x64
```

Run `bin/CyCLAPBridgeStudio.exe` from the installed/extracted tree.

## Linux

Use GCC 11+ or a C++20-capable Clang, Make, CMake 3.27+, OpenGL development
libraries and the X11 development packages required by GLFW. On Debian/Ubuntu,
GLFW documents `xorg-dev` for X11; the OpenGL development package is also needed
(commonly `libgl1-mesa-dev`). See the [GLFW build requirements](https://www.glfw.org/docs/3.4/compile.html).

The prepared preset enables X11 and disables GLFW's Wayland backend. A Wayland
session therefore needs XWayland. Native Wayland integration is not claimed.
Opening local help/results uses `xdg-open`; install the desktop's `xdg-utils`.

```sh
cmake --preset linux
cmake --build --preset linux --parallel
cmake --install build/linux --prefix dist/CyCLAPBridge
cpack --preset linux
```

Run `bin/CyCLAPBridgeStudio` from the installed/extracted tree. Display/runtime
libraries remain OS requirements. Native library/plugin compatibility is unvalidated.

## macOS

Use macOS 11+, Xcode with its SDK/tools, CMake 3.27+ and a C++20-capable Apple
Clang. The GLFW Cocoa backend uses an OpenGL 3.2 core context. Apple deprecates
OpenGL; this implementation has not been validated on a Mac.

```sh
cmake --preset macos
cmake --build --preset macos --parallel
cmake --install build/macos --config Release --prefix dist/CyCLAPBridge
cpack --preset macos
```

The `.app` contains the local manual, notices and conversion sources under
`Contents/Resources/CyCLAPBridge`. The installed CLI can locate that sibling app.
The preset builds the current machine's architecture; build/sign on the target
architecture before distribution. Signing/notarization and AU validation are
not performed. The owner has deferred native macOS testing.

## Library-only and validation

```sh
cmake --preset library-only
cmake --build --preset library-only --config Release
```

This configuration does not initialize or build the Studio dependencies. Existing
audio SDK submodules remain required for the library build and retained notices.

Native tests are opt-in through `-DCYCLAPBRIDGE_BUILD_TESTS=ON`, then run CTest
with `--output-on-failure` and `-C Release` for a multi-configuration generator.
Studio's smoke check renders its forms in both languages and every built-in theme
without a display. Its render check needs a native desktop/OpenGL context. The
controller check covers original fixture generation, overwrite refusal/recovery
and escaped local help URLs. No audio device is opened.

The complete tree is packaged with CPack under `dist/packages`; generated source
projects, private agent instructions, local settings and build logs are not part
of the installation. Package consumers must retain notices and provide their own
plugin redistribution rights.
