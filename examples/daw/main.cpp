// SPDX-License-Identifier: MIT
#include <cyclapbridge/Conversion.h>
#include <cyclapbridge/Plugin.h>
#include <filesystem>
#include <iostream>
#include <vector>
// External-DAW consumer: no SDK header or physical audio device is required.
// A DAW may run this hosting code inside its own worker. The DAW owns IPC and
// process supervision; the plugin instance and buffer views stay in that worker.
namespace {
struct Buffers {
    std::vector<std::vector<float>> planes;
    std::vector<std::vector<float*>> pointers;
    std::vector<cybridge::AudioBus> buses;
    explicit Buffers(const std::vector<cybridge::AudioPort>& ports) {
        for(const auto& port:ports) {
            pointers.emplace_back();
            for(std::uint32_t c=0;c<port.channels;++c) {
                planes.emplace_back(128,0.f);
                pointers.back().push_back(planes.back().data());
            }
        }
        // Plane sample allocations are stable even if their vector objects move.
        for(auto& p:pointers) buses.push_back({p.data(),static_cast<std::uint32_t>(p.size())});
    }
};
int run(const std::filesystem::path& path,cybridge::Format format=cybridge::Format::clap) {
    if(!path.empty()) {
        try {
            cybridge::HostedPlugin plugin(format,std::filesystem::absolute(path));
            Buffers inputs(plugin.inputs()),outputs(plugin.outputs());
            for(auto& plane:inputs.planes) plane[0]=0.25f;
            plugin.prepare(48000,128);
            if(!plugin.start()) return 2;
            cybridge::ProcessBlock block{128,inputs.buses,outputs.buses,{},{},0,false};
            auto result=plugin.process(block);
            plugin.stop(); plugin.release();
            if(result!=cybridge::ProcessStatus::ok) return 3;
            std::cout<<"Processed one prepared block with "<<plugin.descriptor().name<<'\n';
            return 0;
        } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
    }
    for(const auto& capability : cybridge::conversionCapabilities())
        std::cout << cybridge::formatName(capability.source) << " -> "
                  << cybridge::formatName(capability.target) << ": "
                  << (capability.available ? "available" : "planned") << '\n';
    return 0;
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
    auto format=cybridge::Format::clap;
    if(argc>2){auto s=std::filesystem::path(argv[2]).u8string();format=cybridge::parseFormat({reinterpret_cast<const char*>(s.data()),s.size()});}
    return run(argc>1?std::filesystem::path(argv[1]):std::filesystem::path{},format);
}
#else
int main(int argc,char** argv) { return run(argc>1?std::filesystem::path(argv[1]):std::filesystem::path{},argc>2?cybridge::parseFormat(argv[2]):cybridge::Format::clap); }
#endif
