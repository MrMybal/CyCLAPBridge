// SPDX-License-Identifier: MIT
// CyCLAPBridge's local-payload adapter. Upstream clap-wrapper is unmodified.
#include <clap/clap.h>
#include "PayloadName.h"
#include <filesystem>
#include <string>
#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#else
#include <dlfcn.h>
#endif
namespace {
const clap_plugin_entry_t* original = nullptr;
unsigned references = 0;
#if defined(_WIN32)
HMODULE handle = nullptr;
#elif defined(__APPLE__)
CFBundleRef handle = nullptr;
#else
void* handle = nullptr;
#endif
void unload() {
    original = nullptr;
#if defined(_WIN32)
    if(handle) FreeLibrary(handle);
#elif defined(__APPLE__)
    if(handle) { CFBundleUnloadExecutable(handle); CFRelease(handle); }
#else
    if(handle) dlclose(handle);
#endif
    handle = nullptr;
}
bool CLAP_ABI initialize(const char* wrapperPath) {
    if(references) { ++references; return true; }
    try {
        // clap-wrapper supplies the wrapper module path to the linked entry.
        auto wrapper = std::filesystem::u8path(wrapperPath);
#if defined(__APPLE__)
        if(!std::filesystem::is_directory(wrapper)&&wrapper.parent_path().filename()=="MacOS")
            wrapper=wrapper.parent_path().parent_path().parent_path();
        auto payload = wrapper / "Contents/Resources" / std::filesystem::u8path(CYBRIDGE_PAYLOAD_NAME);
        auto text = payload.u8string();
        auto url = CFURLCreateFromFileSystemRepresentation(nullptr,
            reinterpret_cast<const UInt8*>(text.data()), text.size(), true);
        if(url) { handle = CFBundleCreate(nullptr, url); CFRelease(url); }
        if(handle) original = static_cast<const clap_plugin_entry_t*>(
            CFBundleGetDataPointerForName(handle, CFSTR("clap_entry")));
#else
        auto payload = wrapper.parent_path() / std::filesystem::u8path(CYBRIDGE_PAYLOAD_NAME);
#if defined(_WIN32)
        handle = LoadLibraryExW(payload.c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if(handle) original = reinterpret_cast<const clap_plugin_entry_t*>(GetProcAddress(handle, "clap_entry"));
#else
        handle = dlopen(payload.c_str(), RTLD_NOW | RTLD_LOCAL);
        if(handle) original = static_cast<const clap_plugin_entry_t*>(dlsym(handle, "clap_entry"));
#endif
#endif
        if(!original || !original->init || !original->deinit || !original->get_factory ||
            !clap_version_is_compatible(original->clap_version)) { unload(); return false; }
        auto path = payload.u8string();
        if(!original->init(reinterpret_cast<const char*>(path.c_str()))) { unload(); return false; }
        references = 1;
        return true;
    } catch (...) { unload(); return false; }
}
void CLAP_ABI deinitialize() {
    if(!references || --references) return;
    original->deinit();
    unload();
}
const void* CLAP_ABI factory(const char* id) {
    return original ? original->get_factory(id) : nullptr;
}
}
extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry{CLAP_VERSION, initialize, deinitialize, factory};
