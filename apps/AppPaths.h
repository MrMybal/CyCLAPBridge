#pragma once
#include <cyclapbridge/Conversion.h>
#include <filesystem>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif
namespace cybridge::app {
inline std::filesystem::path executablePath() {
#ifdef _WIN32
    std::wstring buffer(32768, L'\0');
    auto count = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    buffer.resize(count); return buffer;
#elif defined(__APPLE__)
    std::uint32_t count = 0; _NSGetExecutablePath(nullptr, &count);
    std::string buffer(count, '\0'); _NSGetExecutablePath(buffer.data(), &count);
    return std::filesystem::canonical(buffer.c_str());
#else
    return std::filesystem::read_symlink("/proc/self/exe");
#endif
}
inline std::filesystem::path backendRoot() {
    auto installed = executablePath().parent_path().parent_path() / "share/CyCLAPBridge";
    if(std::filesystem::exists(installed / "templates/PayloadEntry.cpp")) return installed;
    return std::filesystem::path(CYBRIDGE_SOURCE_ROOT);
}
inline BuildOptions buildOptions() {
    BuildOptions result;
    if(std::filesystem::exists(CYBRIDGE_CMAKE_EXECUTABLE)) result.cmake = CYBRIDGE_CMAKE_EXECUTABLE;
#ifdef _WIN32
    result.generator = "Visual Studio 17 2022";
    result.architecture = "x64";
#endif
    return result;
}
}
