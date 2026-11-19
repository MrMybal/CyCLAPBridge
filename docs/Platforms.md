# Studio platform builds

Studio uses one C++20 interface with CyUI Core/FilePicker/DocumentViews and the native CyMCPSDK chat, one pinned Dear ImGui
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

Studio and the command line compile CyMCPSDK from the sibling checkout if present (on the usual Windows layout, `D:/_Project/CyMCPSDK`). Supply `-DCYCLAPBRIDGE_CYMCP_SOURCE_DIR=<path>` to select a different local checkout. Initialize the pinned `third_party/CyMCPSDK` submodule only as a fallback when local sources are unavailable. CMake prints the source and revision it uses. CyUI may require repository access. The exact revisions and retained licenses
are in `THIRD_PARTY.md`. Studio supplies its own pinned ImGui target; CyUI's
bundled ImGui copy is not compiled. No changes to CyUI are needed.

## Windows

Install Visual Studio 2022 Desktop development with C++, a Windows SDK and CMake
3.27+. Put CMake on PATH for Studio/CLI conversion builds, or select its executable
in Studio's Advanced options. A working OpenGL 3.0 driver and the matching Visual
C++ toolchain are required for plugin conversions. The application package itself
links the MSVC runtime, GLFW and CyUI statically. Its executable starts without
a separate Visual C++ runtime installation.

```sh
cmake --preset windows-x64
cmake --build --preset windows-x64 --parallel
cmake --install build/windows-x64 --config Release --component Studio --prefix dist/CyCLAPBridgeStudio
cpack --preset windows-x64
```

Run `CyCLAPBridgeStudio.exe` directly at the root of the installed/extracted
application. Keep `Resources` beside it. The ZIP contains Studio, the CLI when built, their resources and a short startup guide. The developer SDK supplies the headers and static library.

## Linux

Use GCC 11+ or a C++20-capable Clang, Make, CMake 3.27+, OpenGL development
libraries and the X11 development packages required by GLFW. On Debian/Ubuntu,
GLFW documents `xorg-dev` for X11; the OpenGL development package is also needed
(commonly `libgl1-mesa-dev`). See the [GLFW build requirements](https://www.glfw.org/docs/3.4/compile.html).

The chat build also needs libcurl development files (for example, libcurl4-openssl-dev on Debian/Ubuntu).

The prepared preset enables X11 and disables GLFW's Wayland backend. A Wayland
session therefore needs XWayland. Native Wayland integration is not claimed.
Opening local help/results uses `xdg-open`; install the desktop's `xdg-utils`.

```sh
cmake --preset linux
cmake --build --preset linux --parallel
cmake --install build/linux --component Studio --prefix dist/CyCLAPBridgeStudio
cpack --preset linux
```

Run `CyCLAPBridgeStudio` from the root of the installed/extracted application. Display/runtime
libraries remain OS requirements. Native library/plugin compatibility is unvalidated.

## macOS

Use macOS 11+, Xcode with its SDK/tools, CMake 3.27+ and a C++20-capable Apple
Clang. The chat uses libcurl from the platform or a CMake-discoverable installation. The GLFW Cocoa backend uses an OpenGL 3.2 core context. Apple deprecates
OpenGL; this implementation has not been validated on a Mac.

```sh
cmake --preset macos
cmake --build --preset macos --parallel
cmake --install build/macos --config Release --component Studio --prefix dist/CyCLAPBridgeStudio
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

To package a developer SDK instead of the application, configure with
`CYCLAPBRIDGE_PACKAGE_STUDIO=OFF`. A normal `cmake --install` retains the developer
layout; `--component Studio` explicitly installs the user application. Windows
library-only archive cleanup uses LLVM object tools from Visual Studio's C++
Clang tools; local development can use `CYCLAPBRIDGE_CLEAN_ARCHIVE=OFF`.

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
