// SPDX-License-Identifier: MIT
#include <cyclapbridge/Conversion.h>
#include "Process.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>
namespace cybridge {
namespace detail { ConversionProject createExtendedConversionProject(const ConversionRequest&); }
namespace fs = std::filesystem;
namespace {
std::string cmakeLiteral(const std::string& s) {
    if (s.find_first_of(";\r\n") != std::string::npos || s.find('\0') != std::string::npos)
        throw std::invalid_argument("Paths and names must not contain semicolons, NUL or line breaks");
    std::string equals = "=";
    while (s.find("]" + equals + "]") != std::string::npos) equals += "=";
    return "[" + equals + "[" + s + "]" + equals + "]";
}
void write(const fs::path& path, const std::string& contents) {
    std::ofstream file(path, std::ios::binary);
    file.exceptions(std::ios::failbit | std::ios::badbit);
    file << contents;
}
void require(const fs::path& p) {
    if (!fs::is_regular_file(p)) throw std::runtime_error("Required backend file is missing: " + detail::utf8(p));
}
std::string cppLiteral(const std::string& s) {
    std::string result = "\"";
    for (unsigned char c : s) {
        if(c == '\\' || c == '"') result += '\\';
        if (c < 32) throw std::invalid_argument("Control character in payload filename");
        result += static_cast<char>(c);
    }
    return result + "\"";
}
}
std::string formatName(Format f) {
    switch(f) {
    case Format::clap: return "CLAP";
    case Format::vst3: return "VST3";
    case Format::lv2: return "LV2";
    case Format::auv2: return "AUv2";
    case Format::auv3: return "AUv3";
    case Format::jsfx: return "JSFX";
    }
    throw std::invalid_argument("Unknown format");
}
Format parseFormat(const std::string& s) {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower == "clap") return Format::clap;
    if (lower == "vst3") return Format::vst3;
    if (lower == "lv2") return Format::lv2;
    if (lower == "au" || lower == "auv2") return Format::auv2;
    if (lower == "auv3") return Format::auv3;
    if (lower == "jsfx") return Format::jsfx;
    throw std::invalid_argument("Unknown format: " + s);
}
std::vector<Capability> conversionCapabilities() {
    std::vector<Capability> result;
    for(auto source:{Format::clap,Format::vst3,Format::lv2,Format::jsfx,Format::auv2})
        for(auto target:{Format::clap,Format::vst3,Format::lv2,Format::auv2,Format::auv3,Format::jsfx}) {
            if(source==target)continue;
            if(target==Format::jsfx){result.push_back({source,target,false,"Native plugin to JSFX requires a REAPER native extension; EEL2 has no native-plugin loader"});continue;}
            std::string message="Implemented generator and native backend";
            if(source==Format::auv2||target==Format::auv2||target==Format::auv3)message="Implemented Apple integration; build and runtime validation require macOS";
            result.push_back({source,target,true,message});
        }
    return result;
}
BackendPaths backendFromRoot(const fs::path& root) {
    auto base = fs::absolute(root).lexically_normal();
    return {base / "third_party/clap-wrapper", base / "third_party/clap",
            base / "third_party/vst3sdk", base / "templates",base,base/"third_party/AudioUnitSDK"};
}
ConversionProject createConversionProject(const ConversionRequest& r) {
    if (r.source != Format::clap || r.target != Format::vst3 || !r.resources.empty() || !r.pluginId.empty())
        return detail::createExtendedConversionProject(r);
    if (r.input.empty() || r.projectDirectory.empty()) throw std::invalid_argument("Input and project paths are required");
    auto input = fs::canonical(r.input);
    auto ext = detail::utf8(input.extension());
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (ext != ".clap" || (!fs::is_regular_file(input) && !fs::is_directory(input)))
        throw std::invalid_argument("Input must be a .clap module or bundle");
    auto destination = fs::absolute(r.projectDirectory).lexically_normal();
    cmakeLiteral(detail::utf8(destination));
    if (fs::exists(destination)) throw std::invalid_argument("Project directory already exists; choose a new directory");
    auto name = r.outputName.empty() ? detail::utf8(input.stem()) : r.outputName;
    if (name.empty() || name == "." || name == ".." || name.size() > 120 ||
        name.front() == ' ' || name.back() == ' ' || name.back() == '.' ||
        name.find_first_of("/\\:*?\"<>|$") != std::string::npos)
        throw std::invalid_argument("Output name must be a portable filename without an extension");
    cmakeLiteral(name);
    auto paths = r.backend;
    paths.wrapper = fs::canonical(paths.wrapper);
    paths.clapSdk = fs::canonical(paths.clapSdk);
    paths.vst3Sdk = fs::canonical(paths.vst3Sdk);
    paths.templates = fs::canonical(paths.templates);
    require(paths.wrapper / "CMakeLists.txt");
    require(paths.clapSdk / "include/clap/clap.h");
    require(paths.vst3Sdk / "public.sdk/source/main/pluginfactory.cpp");
    require(paths.vst3Sdk / "pluginterfaces/vst/ivstaudioprocessor.h");
    require(paths.vst3Sdk / "base/source/fobject.cpp");
    require(paths.templates / "PayloadEntry.cpp");
    auto payloadName = detail::utf8(input.filename());
    cmakeLiteral(payloadName);
    if (payloadName.find_first_of("\"$\\") != std::string::npos)
        throw std::invalid_argument("Unsupported payload filename");
    auto parent = destination.parent_path();
    fs::create_directories(parent);
    auto temp = parent / (".cybridge-" + std::to_string(std::random_device{}()) + "-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    if (!fs::create_directory(temp)) throw std::runtime_error("Cannot create project staging directory");
    try {
        fs::create_directory(temp / "payload");
        fs::copy(input, temp / "payload" / input.filename(), fs::copy_options::recursive);
        fs::copy_file(paths.templates / "PayloadEntry.cpp", temp / "PayloadEntry.cpp");
        write(temp / "PayloadName.h", "#pragma once\n#define CYBRIDGE_PAYLOAD_NAME " + cppLiteral(payloadName) + "\n");
        fs::create_directory(temp / "licenses");
        const std::pair<fs::path,const char*> notices[]{
            {paths.wrapper/"LICENSE","clap-wrapper.txt"},
            {paths.clapSdk/"LICENSE","clap.txt"},
            {paths.vst3Sdk/"LICENSE.txt","vst3sdk.txt"},
            {paths.vst3Sdk/"base/LICENSE.txt","vst3-base.txt"},
            {paths.vst3Sdk/"pluginterfaces/LICENSE.txt","vst3-interfaces.txt"},
            {paths.vst3Sdk/"public.sdk/LICENSE.txt","vst3-public-sdk.txt"},
            {paths.wrapper/"libs/fmt/fmt/format.h","fmt-format.h"},
            {paths.wrapper/"libs/psl/presonus-plugin-extensions/ipslgainreduction.h","presonus-extensions.h"},
            {paths.templates.parent_path()/"LICENSE","CyCLAPBridge.txt"},
            {paths.templates.parent_path()/"docs/Licensing.md","Licensing.md"}
        };
        for(const auto& [file,label] : notices) {
            require(file); fs::copy_file(file,temp/"licenses"/label);
        }
        std::ostringstream cmake;
        cmake << "cmake_minimum_required(VERSION 3.27)\n"
                 "project(CyCLAPBridgeConversion VERSION 0.1.0 LANGUAGES C CXX)\n"
                 "set(CMAKE_MSVC_RUNTIME_LIBRARY \"MultiThreaded$<$<CONFIG:Debug>:Debug>\")\n"
                 "set(CMAKE_CXX_STANDARD 20)\nset(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
                 "set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15 CACHE STRING \"\")\n"
                 "set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES OFF CACHE BOOL \"\" FORCE)\n"
                 "set(CLAP_WRAPPER_COPY_AFTER_BUILD OFF CACHE BOOL \"\" FORCE)\n"
                 "set(CLAP_WRAPPER_BUILD_TESTS OFF CACHE BOOL \"\" FORCE)\n"
                 "set(CLAP_WRAPPER_BUILD_AAX OFF CACHE BOOL \"\" FORCE)\n"
                 "set(CLAP_WRAPPER_BUILD_STANDALONE OFF CACHE BOOL \"\" FORCE)\n"
                 "set(CLAP_WRAPPER_OUTPUT_NAME " << cmakeLiteral(name) << ")\n"
              << "set(CLAP_SDK_ROOT " << cmakeLiteral(detail::utf8(paths.clapSdk)) << " CACHE PATH \"\" FORCE)\n"
              << "set(VST3_SDK_ROOT " << cmakeLiteral(detail::utf8(paths.vst3Sdk)) << " CACHE PATH \"\" FORCE)\n"
              << "add_subdirectory(" << cmakeLiteral(detail::utf8(paths.wrapper)) << " wrapper EXCLUDE_FROM_ALL)\n"
                 "add_library(cybridge_wrapper MODULE PayloadEntry.cpp)\n"
                 "target_add_vst3_wrapper(TARGET cybridge_wrapper OUTPUT_NAME \"${CLAP_WRAPPER_OUTPUT_NAME}\" WINDOWS_FOLDER_VST3 FALSE)\n"
                 "target_compile_definitions(clap-wrapper-shared-detail PRIVATE STATICALLY_LINKED_CLAP_ENTRY=1)\n"
                 "target_include_directories(cybridge_wrapper PRIVATE \"${CMAKE_CURRENT_SOURCE_DIR}\")\n"
                 "if(MSVC)\n  target_compile_options(cybridge_wrapper PRIVATE /utf-8)\nendif()\n"
                 "set_target_properties(cybridge_wrapper PROPERTIES PREFIX \"\")\n"
                 "set_target_properties(cybridge_wrapper PROPERTIES LIBRARY_OUTPUT_DIRECTORY \"${CMAKE_BINARY_DIR}/artifacts/$<CONFIG>\" RUNTIME_OUTPUT_DIRECTORY \"${CMAKE_BINARY_DIR}/artifacts/$<CONFIG>\")\n"
                 "if(APPLE)\n  set(_payload_destination \"$<TARGET_BUNDLE_DIR:cybridge_wrapper>/Contents/Resources\")\n"
                 "else()\n  set(_payload_destination \"$<TARGET_FILE_DIR:cybridge_wrapper>\")\nendif()\n";
        cmake << "add_custom_command(TARGET cybridge_wrapper POST_BUILD\n"
                 "  COMMAND ${CMAKE_COMMAND} -E make_directory \"${_payload_destination}\"\n"
                 "  COMMAND ${CMAKE_COMMAND} -E ";
        cmake << (fs::is_directory(input) ? "copy_directory" : "copy_if_different");
        cmake << " \"${CMAKE_CURRENT_SOURCE_DIR}/payload/" << payloadName << "\" \"${_payload_destination}/" << payloadName << "\"\n"
                 "  COMMAND ${CMAKE_COMMAND} -E copy_directory \"${CMAKE_CURRENT_SOURCE_DIR}/licenses\" \"${_payload_destination}/CyCLAPBridge-licenses\"\n"
                 "  VERBATIM)\n";
        write(temp / "CMakeLists.txt", cmake.str());
        write(temp / "cybridge-project.txt", "CyCLAPBridge 0.1.0\nCLAP -> VST3\nPayload: " + payloadName +
            "\nOutput: " + name + "\nBackend: clap-wrapper 806bba0a5f792e3966b99c1561ab9952d3316d7c\n"
            "Keep the payload beside the generated binary (inside Resources on macOS).\n"
            "The wrapper loads plugin code in the host process selected by the DAW.\n"
            "The DAW owns process isolation, crash recovery and security sandboxing.\n");
        if (fs::exists(destination)) throw std::runtime_error("Project destination appeared during generation");
        fs::rename(temp, destination);
    } catch (...) {
        std::error_code ignored;
        fs::remove_all(temp, ignored);
        throw;
    }
    return {destination, name};
}
BuildResult buildConversionProject(const ConversionProject& project, const BuildOptions& options,
                                   const LogSink& log) {
    require(project.directory / "CMakeLists.txt");
    if (options.configuration.empty() || options.configuration.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") != std::string::npos)
        throw std::invalid_argument("Invalid build configuration");
    auto build = project.directory / "build";
    std::vector<std::string> configure{"-S", detail::utf8(project.directory), "-B", detail::utf8(build),
                                      "-DCMAKE_BUILD_TYPE=" + options.configuration};
    if (!options.generator.empty()) { configure.emplace_back("-G"); configure.push_back(options.generator); }
    if (!options.architecture.empty()) { configure.emplace_back("-A"); configure.push_back(options.architecture); }
    auto code = detail::runProcess(options.cmake, configure, log);
    std::string target="cybridge_wrapper";
    if(fs::exists(project.directory/"cybridge-target.txt")){
        std::ifstream file(project.directory/"cybridge-target.txt");std::getline(file,target);
        if(target.empty()||target.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")!=std::string::npos)
            throw std::runtime_error("Invalid generated build target");
    }
    if (!code) code = detail::runProcess(options.cmake,
        {"--build", detail::utf8(build), "--config", options.configuration, "--target", target, "--parallel", "2"}, log);
    return {code, build / "artifacts" / options.configuration};
}
}
