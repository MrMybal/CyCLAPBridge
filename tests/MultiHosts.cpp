// SPDX-License-Identifier: MIT
#include <cyclapbridge/Plugin.h>
#include <array>
#include <iostream>
#include <cmath>
int run(const std::string& format,const std::filesystem::path& path){
    try{
        cybridge::HostedPlugin plugin(cybridge::parseFormat(format),std::filesystem::absolute(path));
        if(plugin.inputs().size()!=1||plugin.outputs().size()!=1||plugin.inputs()[0].channels!=2||plugin.outputs()[0].channels!=2||plugin.parameters().empty())throw std::runtime_error("Fixture metadata mismatch");
        auto param=plugin.parameters()[0];
        plugin.prepare(48000,64);if(!plugin.start())throw std::runtime_error("Start failed");
        std::array<float,64> a,b,c,d;a.fill(.25f);b.fill(-.5f);c.fill(0);d.fill(0);
        float* in[]{a.data(),b.data()},*out[]{c.data(),d.data()};
        std::array<cybridge::AudioBus,1> inputs{{{in,2}}},outputs{{{out,2}}};
        cybridge::Event event;event.parameterId=param.id;event.frame=17;event.value=param.maximum==1?.25:.5;
        std::array<cybridge::Event,32> received;
        cybridge::ProcessBlock block{64,inputs,outputs,std::span(&event,1),received};
        if(plugin.process(block)!=cybridge::ProcessStatus::ok)throw std::runtime_error("Processing failed");
        if(std::abs(c[16]-.25f)>1e-6||std::abs(c[17]-.125f)>1e-6||std::abs(d[63]+.25f)>1e-6) {
            std::cerr<<"samples: "<<c[16]<<" "<<c[17]<<" "<<d[63]<<'\n';
            throw std::runtime_error("Audio/automation mismatch");
        }
        plugin.stop();
        auto state=plugin.saveState();plugin.loadState(state);plugin.release();
        {
            cybridge::HostedPlugin restored(cybridge::parseFormat(format),std::filesystem::absolute(path));
            restored.loadState(state); // DAWs restore sessions before activation.
            restored.prepare(48000,64);
            if(!restored.start())throw std::runtime_error("Restored start failed");
            c.fill(0);d.fill(0);
            block.events={};
            if(restored.process(block)!=cybridge::ProcessStatus::ok||std::abs(c[63]-.125f)>1e-6)
                throw std::runtime_error("Restore before activation mismatch");
            restored.stop();restored.release();
        }
        std::cout<<"PASS: "<<format<<" audio, automation, state: "<<plugin.descriptor().name<<'\n';return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){
    if(argc!=3)return 2;auto u=std::filesystem::path(argv[1]).u8string();
    return run(std::string(reinterpret_cast<const char*>(u.data()),u.size()),argv[2]);
}
#else
int main(int argc,char** argv){return argc==3?run(argv[1],argv[2]):2;}
#endif
