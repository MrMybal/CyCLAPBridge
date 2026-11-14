// SPDX-License-Identifier: MIT
#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <vector>
namespace cybridge {
enum class Format { clap, vst3, lv2, auv2, auv3, jsfx };
struct Capability { Format source, target; bool available; std::string description; };
std::vector<Capability> conversionCapabilities();
std::string formatName(Format);
Format parseFormat(const std::string&);
struct BackendPaths {
    std::filesystem::path wrapper, clapSdk, vst3Sdk, templates;
    std::filesystem::path projectRoot, audioUnitSdk;
};
// No download, scan or plugin installation.
BackendPaths backendFromRoot(const std::filesystem::path&);
struct ConversionRequest {
    std::filesystem::path input, projectDirectory;
    Format source = Format::clap, target = Format::vst3;
    std::string outputName;
    BackendPaths backend;
    std::string pluginId; // VST3 class ID, LV2 URI or CLAP ID; empty chooses the first.
    std::string bundleId = "org.cybridge.generated";
    std::string manufacturer = "CyBr", subtype; // AU four-character codes.
    std::filesystem::path resources; // Optional source asset/import root, copied with the plugin.
};
struct ConversionProject { std::filesystem::path directory; std::string outputName; };
// Control thread. Creates a new directory; refuses to overwrite existing data.
// Copies the CLAP payload and emits an offline CMake wrapper project.
// Throws std::exception on invalid/unsupported requests or I/O failure.
ConversionProject createConversionProject(const ConversionRequest&);
using LogSink = std::function<void(const std::string&)>;
struct BuildOptions {
    std::filesystem::path cmake = "cmake";
    std::string configuration = "Release", generator, architecture;
};
struct BuildResult { int exitCode = -1; std::filesystem::path artifactsDirectory; };
// Control/background thread. Runs CMake directly, without a command shell.
BuildResult buildConversionProject(const ConversionProject&, const BuildOptions& = {},
                                   const LogSink& = {});
}
