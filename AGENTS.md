# CyCLAPBridge development

C++20, namespace cybridge, public headers under include/cyclapbridge.
This project is standalone: no dependency on CyAudioCore, CyDSP, CyDAW or CyUI.
Keep plugin SDKs private and the library independent of a UI toolkit.
The native Windows Studio is a separate optional executable.
Production library is static; fixtures and native tests are explicit opt-in.
Preserve exact upstream notices and pinned submodule provenance.
Update the capability matrix when implementing a direction; never claim an
unsupported conversion as functional.
The DAW owns worker processes, IPC, crash recovery and security sandboxing.
CyCLAPBridge executes inside the host process selected by the DAW; process
management is outside this project's scope, with no dependency on a specific DAW.
Prepared host audio must not allocate, free, wait or perform I/O.
Foreign plugins retain their own real-time behavior.
The owner deferred native macOS testing because no macOS machine is available.
Keep Apple integrations clearly marked as unvalidated; do not count those tests
as completed or require them to deliver the Windows implementation.
Native validation uses original fixtures; no physical devices, publishing,
pushes, CI or antivirus changes.
