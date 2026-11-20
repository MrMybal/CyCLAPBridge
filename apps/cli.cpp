// SPDX-License-Identifier: MIT
#include <cyclapbridge/Conversion.h>
#include <cyclapbridge/Plugin.h>
#include "AppPaths.h"
#include <iostream>
#include <map>
#include <stdexcept>

namespace cybridge::app {
int runTools(const std::vector<std::string>&);
std::filesystem::path cliBackendRoot(const BuildOptions&);
}
namespace {
std::string text(const std::filesystem::path& p) {
    auto u = p.u8string(); return {reinterpret_cast<const char*>(u.data()), u.size()};
}
std::filesystem::path path(const std::string& s) {
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(s.data()),s.size()));
}
void usage() {
    std::cout << "CyCLAPBridge 0.2.2 - C++ plugin interoperability\n\n"
      "  capabilities\n"
      "  tools list | tools describe NAME | tools call NAME (tools --help)\n"
      "  convert --input Plugin.clap --output NEW_PROJECT [--name NAME] [--build]\n"
      "          [--from clap|vst3|lv2|jsfx|au] [--to clap|vst3|lv2|au|auv3]\n"
      "          [--backend-root PATH] [--plugin-id ID] [--resources DIR]\n"
      "          [--au-sdk DIR] [--bundle-id ID]\n"
      "  build --project PROJECT\n"
      "  inspect --input ABSOLUTE_PLUGIN.clap [--plugin-id ID]\n\n"
      "Build options: --cmake PATH --generator NAME --arch x64 --config Release\n"
      "Inspect executes plugin code in this process. Conversion generation copies files only.\n"
      "Conversion requires CMake and a native C++20 compiler. No automatic plugin installation.\n";
}
int run(const std::vector<std::string>& args) {
    try {
        if(args.size() < 2 || args[1] == "--help" || args[1] == "help") { usage(); return 0; }
        if(args[1] == "tools") return cybridge::app::runTools(args);
        if(args[1] == "capabilities") {
            for(auto& c : cybridge::conversionCapabilities())
                std::cout << cybridge::formatName(c.source) << " -> " << cybridge::formatName(c.target)
                  << " : " << (c.available ? "available" : "planned") << " | " << c.description << '\n';
            return 0;
        }
        std::map<std::string,std::string> options;
        bool build = false;
        for(std::size_t i = 2; i < args.size(); ++i) {
            if(args[i] == "--build") { build = true; continue; }
            const auto& key = args[i];
            if(key != "--input" && key != "--output" && key != "--name" && key != "--from" &&
               key != "--to" && key != "--backend-root" && key != "--cmake" && key != "--generator" &&
               key != "--arch" && key != "--config" && key != "--project" && key != "--plugin-id" &&
               key != "--resources" && key != "--au-sdk" && key != "--bundle-id")
                throw std::invalid_argument("Unknown option: " + key);
            if(i + 1 == args.size()) throw std::invalid_argument("Missing value for " + key);
            if(!options.emplace(key, args[++i]).second) throw std::invalid_argument("Duplicate option: " + key);
        }
        auto required = [&](const std::string& key) {
            if(!options.contains(key)) throw std::invalid_argument("Required option: " + key);
            return path(options.at(key));
        };
        if(args[1] == "inspect") {
            auto format=options.contains("--from")?cybridge::parseFormat(options["--from"]):cybridge::Format::clap;
            cybridge::HostedPlugin plugin(format,required("--input"), options["--plugin-id"]);
            const auto& d = plugin.descriptor();
            std::cout << d.name << '\n' << d.id << '\n' << d.vendor << " " << d.version << '\n'
                      << plugin.inputs().size() << " input buses, " << plugin.outputs().size()
                      << " output buses, " << plugin.parameters().size() << " parameters\n";
            return 0;
        }
        auto config = cybridge::app::buildOptions();
        if(options.contains("--cmake")) config.cmake = path(options["--cmake"]);
        if(options.contains("--generator")) config.generator = options["--generator"];
        if(options.contains("--arch")) config.architecture = options["--arch"];
        if(options.contains("--config")) config.configuration = options["--config"];
        cybridge::ConversionProject project;
        if(args[1] == "convert") {
            cybridge::ConversionRequest request;
            request.input = required("--input"); request.projectDirectory = required("--output");
            request.outputName = options["--name"];
            if(options.contains("--from")) request.source = cybridge::parseFormat(options["--from"]);
            if(options.contains("--to")) request.target = cybridge::parseFormat(options["--to"]);
            request.backend = cybridge::backendFromRoot(options.contains("--backend-root") ?
                path(options["--backend-root"]) : cybridge::app::cliBackendRoot(config));
            request.pluginId=options["--plugin-id"];
            if(options.contains("--resources"))request.resources=path(options["--resources"]);
            if(options.contains("--au-sdk"))request.backend.audioUnitSdk=path(options["--au-sdk"]);
            if(options.contains("--bundle-id"))request.bundleId=options["--bundle-id"];
            project = cybridge::createConversionProject(request);
            std::cout << "Project: " << text(project.directory) << '\n';
            if(!build) return 0;
        } else if(args[1] == "build") {
            project.directory = std::filesystem::absolute(required("--project"));
        } else throw std::invalid_argument("Unknown command: " + args[1]);
        auto result = cybridge::buildConversionProject(project, config,
            [](const std::string& line) { std::cout << line << std::flush; });
        if(!result.exitCode) std::cout << "\nArtifacts: " << text(result.artifactsDirectory) << '\n';
        return result.exitCode;
    } catch(const std::exception& e) { std::cerr << "CyCLAPBridge: " << e.what() << '\n'; return 1; }
}
}
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    std::vector<std::string> args;
    for(int i = 0; i < argc; ++i) args.push_back(text(argv[i]));
    return run(args);
}
#else
int main(int argc, char** argv) { return run({argv, argv + argc}); }
#endif
