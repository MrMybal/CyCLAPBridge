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
inline std::filesystem::path dataRoot() {
    const auto directory=executablePath().parent_path();
    for(const auto& candidate:{directory/"Resources",directory/"../Resources/CyCLAPBridge",
            directory/"../share/CyCLAPBridge",
            directory/"../CyCLAPBridgeStudio.app/Contents/Resources/CyCLAPBridge"})
        if(std::filesystem::is_regular_file(candidate/"Documentation/Documentation.html"))
            return std::filesystem::weakly_canonical(candidate);
    // Development checkout discovery is relative to the executable, never an
    // absolute build-machine path embedded into distributed binaries.
    auto parent=directory;
    for(int i=0;i<6;++i){
        if(std::filesystem::is_regular_file(parent/"templates/PayloadEntry.cpp"))return parent;
        auto next=parent.parent_path();if(next==parent)break;parent=next;
    }
    return std::filesystem::current_path();
}
inline std::filesystem::path backendRoot() { return dataRoot(); }
inline BuildOptions buildOptions() {
    BuildOptions result;
#ifdef _WIN32
    std::wstring directory(32768,L'\0');auto count=GetEnvironmentVariableW(L"ProgramFiles",directory.data(),static_cast<DWORD>(directory.size()));
    if(count&&count<directory.size()){
        directory.resize(count);const std::filesystem::path base(directory);
        auto standalone=base/L"CMake/bin/cmake.exe";
        if(std::filesystem::is_regular_file(standalone))result.cmake=standalone;
        else for(const auto* edition:{L"Community",L"Professional",L"Enterprise",L"BuildTools"}){
            auto bundled=base/L"Microsoft Visual Studio/2022"/edition/L"Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe";
            if(std::filesystem::is_regular_file(bundled)){result.cmake=bundled;break;}
        }
    }
    result.generator = "Visual Studio 17 2022";
    result.architecture = "x64";
#elif defined(__APPLE__)
    for(const auto* path:{"/Applications/CMake.app/Contents/bin/cmake","/opt/homebrew/bin/cmake","/usr/local/bin/cmake"})
        if(std::filesystem::is_regular_file(path)){result.cmake=path;break;}
#endif
    return result;
}
}
