// SPDX-License-Identifier: MIT
// Explicit Windows-only acceptance: original generated plugin, no devices.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <pluginterfaces/base/ipluginbase.h>
#include <pluginterfaces/vst/ivsthostapplication.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/vstspeaker.h>
#include <pluginterfaces/vst/ivstparameterchanges.h>
#include <array>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
void check(bool b,const char* message) { if(!b) throw std::runtime_error(message); }
struct Host final : IHostApplication {
    tresult PLUGIN_API queryInterface(const TUID id,void** out) override {
        *out=nullptr;
        if(!std::memcmp(id,IHostApplication::iid,16) || !std::memcmp(id,FUnknown::iid,16)) {
            *out=static_cast<IHostApplication*>(this); return kResultOk;
        }
        return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override { return 1; }
    uint32 PLUGIN_API release() override { return 1; }
    tresult PLUGIN_API getName(String128 out) override {
        const char16_t* text=u"CyCLAPBridge Tests";
        std::copy(text,text+19,out); return kResultOk;
    }
    tresult PLUGIN_API createInstance(TUID,TUID,void** out) override { *out=nullptr; return kNoInterface; }
};
struct Queue final : IParamValueQueue {
    tresult PLUGIN_API queryInterface(const TUID,void** out) override { *out=nullptr; return kNoInterface; }
    uint32 PLUGIN_API addRef() override { return 1; }
    uint32 PLUGIN_API release() override { return 1; }
    ParamID PLUGIN_API getParameterId() override { return 7; }
    int32 PLUGIN_API getPointCount() override { return 1; }
    tresult PLUGIN_API getPoint(int32 n,int32& frame,ParamValue& value) override {
        if(n) return kInvalidArgument; frame=17; value=0.25; return kResultOk;
    }
    tresult PLUGIN_API addPoint(int32,ParamValue,int32&) override { return kNotImplemented; }
};
struct Changes final : IParameterChanges {
    Queue queue;
    tresult PLUGIN_API queryInterface(const TUID,void** out) override { *out=nullptr; return kNoInterface; }
    uint32 PLUGIN_API addRef() override { return 1; }
    uint32 PLUGIN_API release() override { return 1; }
    int32 PLUGIN_API getParameterCount() override { return 1; }
    IParamValueQueue* PLUGIN_API getParameterData(int32 n) override { return n ? nullptr : &queue; }
    IParamValueQueue* PLUGIN_API addParameterData(const ParamID&,int32&) override { return nullptr; }
};
}
int wmain(int argc,wchar_t** argv) {
    try {
        check(argc==2,"Expected generated VST3 path");
        auto path=std::filesystem::absolute(argv[1]);
        auto module=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        check(module!=nullptr,"LoadLibrary VST3");
        auto init=reinterpret_cast<bool(*)()>(GetProcAddress(module,"InitDll"));
        auto exit=reinterpret_cast<bool(*)()>(GetProcAddress(module,"ExitDll"));
        auto factoryEntry=reinterpret_cast<IPluginFactory*(*)()>(GetProcAddress(module,"GetPluginFactory"));
        check(init && exit && factoryEntry && init(),"VST3 entrypoints");
        auto* factory=factoryEntry();
        check(factory && factory->countClasses()>0,"VST3 factory did not find adjacent CLAP payload");
        PClassInfo info{};
        bool found=false;
        for(int32 n=0;n<factory->countClasses();++n) {
            factory->getClassInfo(n,&info);
            if(!std::strcmp(info.category,kVstAudioEffectClass)) { found=true; break; }
        }
        check(found,"VST3 audio component class");
        IComponent* component=nullptr;
        check(factory->createInstance(info.cid,IComponent::iid,reinterpret_cast<void**>(&component))==kResultOk,"Create VST3 component");
        Host host;
        check(component->initialize(&host)==kResultOk,"Initialize VST3 component");
        IAudioProcessor* processor=nullptr;
        check(component->queryInterface(IAudioProcessor::iid,reinterpret_cast<void**>(&processor))==kResultOk,"VST3 processor");
        SpeakerArrangement stereo=SpeakerArr::kStereo;
        check(processor->setBusArrangements(&stereo,1,&stereo,1)==kResultOk,"VST3 stereo layout");
        ProcessSetup setup{kRealtime,kSample32,64,48000};
        check(processor->setupProcessing(setup)==kResultOk,"VST3 processing setup");
        component->activateBus(kAudio,kInput,0,true); component->activateBus(kAudio,kOutput,0,true);
        check(component->setActive(true)==kResultOk,"VST3 activation");
        check(processor->setProcessing(true)==kResultOk,"VST3 start");
        std::array<float,64> a,b,c,d;
        a.fill(0.25f); b.fill(-0.5f); c.fill(0); d.fill(0);
        float* in[]{a.data(),b.data()},*out[]{c.data(),d.data()};
        AudioBusBuffers input{},output{};
        input.numChannels=output.numChannels=2; input.channelBuffers32=in; output.channelBuffers32=out;
        Changes changes;
        ProcessData data{};
        data.processMode=kRealtime; data.symbolicSampleSize=kSample32; data.numSamples=64;
        data.numInputs=data.numOutputs=1; data.inputs=&input; data.outputs=&output;
        data.inputParameterChanges=&changes;
        check(processor->process(data)==kResultOk,"VST3 audio call");
        check(c[16]==0.25f && c[17]==0.125f && d[63]==-0.25f,"Wrapped audio or automation mismatch");
        processor->setProcessing(false); component->setActive(false);
        processor->release(); component->terminate(); component->release(); factory->release();
        check(exit(),"VST3 module shutdown");
        FreeLibrary(module);
        std::cout<<"PASS: VST3 factory, adjacent CLAP payload, audio, sample automation, clean unload\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
}
