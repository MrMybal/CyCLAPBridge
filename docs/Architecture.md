# Architecture

CyCLAPBridge is a standalone project; it does not import another Cy project.

    CyCLAPBridgeStudio (Win32 C++)    cyclapbridge-cli (C++)
                  \                  /
                   CyCLAPBridge static library
                    /                    \
       Conversion project generation    Format-neutral HostedPlugin
                 |                            |
       pinned clap-wrapper + SDKs        Native module in DAW-selected process
                 |
          CLAP / VST3 / LV2 / AU + local payload and assets

The SDKs remain private implementation details. The static library includes CLAP,
VST3 interface definitions, Lilv's RDF stack and the ysfx/EEL2 runtime. Apple
hosting is conditional on macOS. Studio and its Win32 UI are separate.
No other Cy project is linked. clap-wrapper supplies VST3/AU exports;
original adapters supply the native-to-CLAP facade and LV2 export.
The generated LV2 metadata tool runs in a separate build-time process.

Conversion generation validates paths and the supported direction, copies the
selected payload into a fresh project, and publishes the project directory after
the writes finish. Existing destinations are refused. It does not load a plugin.
Build operations call CMake without cmd.exe, PowerShell or a Unix shell.
Windows build processes inherit only their own redirected standard handles.
The caller owns the requested build; build operations are blocking control work.

The original PayloadEntry adapter delegates the linked CLAP entry to a module
next to the VST3 binary (Resources on macOS). Upstream source is unmodified.
Plugin identities and format translations belong to clap-wrapper.
The sidecar layout does not eliminate a third-party plugin's external resources,
runtime dependencies or registration requirements.

Direct hosting stores bounded prepared native buffers and input event storage.
Editor.h describes native parent handles, resize and automation callbacks without
importing SDK types or a UI toolkit. Editor operations run on the creator/UI
thread. Prepared parameter mirrors transfer GUI values to the audio owner and
host automation back to the native editor through pumpMainThread().
Generated CLAP facades forward clap.gui and use the host timer extension for GUI
idle/graphics work. Native state envelopes distinguish pending GUI values from
the plugin's saved state. See Editors.md for lifecycle and platform coverage.
Caller-provided output event storage reports overflow and unsupported events.
State streams are bounded to 64 MiB and respect short read/EOF behavior.
Foreign state loads may partially mutate a plugin before returning failure.
Shared module ownership keeps the entry alive until the last instance disappears.

The DAW owns process creation, IPC, audio/event transport, worker supervision,
crash recovery and security sandboxing. These are outside CyCLAPBridge's scope.
The DAW can link the static library into a worker and create HostedPlugin there,
or load a generated CLAP/VST3/LV2/AU wrapper using its existing plugin host.
The same adapters can run in the DAW's main process if that is its chosen policy.
No particular DAW or process protocol is required by this library.

Construct and destroy each instance inside its hosting process, respecting the
creator/control-thread and audio-thread contract. Objects, SDK handles, pointers
and ProcessBlock spans are process-local. The DAW maps its own transport buffers
to local views before calling process; it must not send these C++ objects across
the process boundary. State bytes can be transferred by the DAW and restored on
the control thread of a newly created instance after a worker restart.

IPC deadlines, shared-storage preparation, pipeline latency, late-block handling
and offline-rendering policy belong to the DAW. The prepared adapter contract
applies inside the selected process; foreign plugin code retains its own behavior.
Process isolation and OS privilege restrictions are guarantees of the DAW's
implementation and are not supplied by the adapter library.

## Optional portable Studio

Studio is a separate consumer of the static library. Its CyUI Core/FilePicker,
single pinned ImGui target and GLFW/OpenGL backend do not enter the library's
public headers or CMake export. The same interface and background conversion
controller serve Windows, Linux X11 and macOS. Platform helpers own local help
URLs, filesystem roots and user preferences. The DAW's process-management
responsibilities are unchanged. See [platform build layouts](Platforms.md).
