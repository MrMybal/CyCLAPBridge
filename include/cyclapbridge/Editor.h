// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
namespace cybridge {
enum class WindowApi { win32, cocoa, x11 };
struct NativeWindow { WindowApi api; void* handle; };
struct EditorSize { std::uint32_t width=640, height=480; };
enum class EditGesture { begin, value, end };
// Creator/UI thread only. Keep context alive until destroyEditor() completes.
// Callback functions must not throw. Returning false declines a resize.
struct EditorCallbacks {
    void* context=nullptr;
    bool (*requestResize)(void*,EditorSize) noexcept=nullptr;
    void (*parameterEdit)(void*,std::uint32_t,double,EditGesture) noexcept=nullptr;
    void (*closed)(void*) noexcept=nullptr;
};
}
