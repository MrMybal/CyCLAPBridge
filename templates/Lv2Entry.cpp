// SPDX-License-Identifier: MIT
#include <cyclapbridge/Plugin.h>
#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/midi/midi.h>
#include <lv2/urid/urid.h>
#include <lv2/state/state.h>
#include "NativeConfig.h"
#include "Lv2Metadata.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
namespace {
struct Instance {
    cybridge::HostedPlugin plugin;
    std::vector<void*> ports;
    std::vector<float> silence;
    std::vector<std::vector<float*>> inputPointers,outputPointers;
    std::vector<cybridge::AudioBus> inputs,outputs;
    std::vector<cybridge::Event> events,received;
    std::vector<double> previous;
    LV2_URID midi=0,sequence=0,chunk=0,stateKey=0;
    bool active=false;
    Instance(double rate,const std::filesystem::path& bundle,LV2_URID_Map& map):
        plugin(cybridge::parseFormat(CYBRIDGE_SOURCE_FORMAT),bundle/std::filesystem::u8path(CYBRIDGE_SOURCE_FILE),CYBRIDGE_SOURCE_ID){
        auto n=CYBRIDGE_INPUT_CHANNELS+CYBRIDGE_OUTPUT_CHANNELS+CYBRIDGE_PARAMETER_COUNT+2;
        ports.resize(n);silence.resize(65536);events.resize(4096);received.resize(4096);
        previous.resize(plugin.parameters().size());
        for(std::size_t i=0;i<previous.size();++i)previous[i]=plugin.parameters()[i].defaultValue;
        auto setup=[](const auto& source,auto& ptrs,auto& buses){
            ptrs.resize(source.size());for(std::size_t i=0;i<source.size();++i){ptrs[i].resize(source[i].channels);buses.push_back({ptrs[i].data(),source[i].channels});}
        };
        setup(plugin.inputs(),inputPointers,inputs);setup(plugin.outputs(),outputPointers,outputs);
        plugin.prepare(rate,65536,4096);
        midi=map.map(map.handle,LV2_MIDI__MidiEvent);sequence=map.map(map.handle,LV2_ATOM__Sequence);
        chunk=map.map(map.handle,LV2_ATOM__Chunk);stateKey=map.map(map.handle,"urn:cybridge:native-state");
    }
};
LV2_Handle instantiate(const LV2_Descriptor*,double rate,const char* bundle,const LV2_Feature*const* features){
    LV2_URID_Map* map=nullptr;
    if(features)for(auto p=features;*p;++p)if(!std::strcmp((*p)->URI,LV2_URID__map))map=static_cast<LV2_URID_Map*>((*p)->data);
    if(!map)return nullptr;
    try{return new Instance(rate,std::filesystem::u8path(bundle),*map);}catch(...){return nullptr;}
}
void connect(LV2_Handle p,std::uint32_t n,void* data){auto& s=*static_cast<Instance*>(p);if(n<s.ports.size())s.ports[n]=data;}
void activate(LV2_Handle p){auto& s=*static_cast<Instance*>(p);s.active=s.plugin.start();}
void deactivate(LV2_Handle p){auto& s=*static_cast<Instance*>(p);s.plugin.stop();s.active=false;}
void run(LV2_Handle p,std::uint32_t frames){
    auto& s=*static_cast<Instance*>(p);
    if(!s.active||!frames||frames>65536)return;
    std::uint32_t port=0;
    for(auto& bus:s.inputPointers)for(auto& channel:bus)channel=s.ports[port]?static_cast<float*>(s.ports[port++]):(port++,s.silence.data());
    for(auto& bus:s.outputPointers)for(auto& channel:bus)channel=s.ports[port]?static_cast<float*>(s.ports[port++]):(port++,s.silence.data());
    std::size_t count=0;
    for(std::size_t i=0;i<s.plugin.parameters().size();++i,++port)if(s.ports[port]){
        auto value=*static_cast<float*>(s.ports[port]);const auto& param=s.plugin.parameters()[i];
        if(value!=s.previous[i]&&std::isfinite(value)&&value>=param.minimum&&value<=param.maximum){
            if(count<s.events.size()){auto& e=s.events[count++];e={};e.parameterId=param.id;e.value=value;s.previous[i]=value;}
        }
    }
    auto* incoming=static_cast<const LV2_Atom_Sequence*>(s.ports[port++]);
    auto* outgoing=static_cast<LV2_Atom_Sequence*>(s.ports[port]);
    std::uint32_t capacity=outgoing?outgoing->atom.size:0;
    if(outgoing){outgoing->atom={sizeof(LV2_Atom_Sequence_Body),s.sequence};outgoing->body={};}
    if(incoming&&incoming->atom.type==s.sequence&&incoming->atom.size>=sizeof(incoming->body)&&incoming->atom.size<=1024*1024){
        auto* cursor=reinterpret_cast<const std::byte*>(&incoming->body+1);
        auto* finish=reinterpret_cast<const std::byte*>(&incoming->atom+1)+incoming->atom.size;
        while(finish-cursor>=sizeof(LV2_Atom_Event)){
            auto* e=reinterpret_cast<const LV2_Atom_Event*>(cursor);
            auto total=sizeof(LV2_Atom_Event)+static_cast<std::uint64_t>(lv2_atom_pad_size(e->body.size));
            if(total>static_cast<std::uint64_t>(finish-cursor))break;
            if(e->body.type==s.midi&&e->body.size==3&&e->time.frames>=0&&e->time.frames<frames&&count<s.events.size()){
                auto& n=s.events[count++];n={};n.kind=cybridge::EventKind::midi;n.frame=static_cast<std::uint32_t>(e->time.frames);std::memcpy(n.midi,e+1,3);
            }cursor+=total;
        }
    }
    std::sort(s.events.begin(),s.events.begin()+count,[](const auto& a,const auto& b){return a.frame<b.frame;});
    cybridge::ProcessBlock block{frames,s.inputs,s.outputs,std::span(s.events.data(),count),s.received};
    if(s.plugin.process(block)!=cybridge::ProcessStatus::ok){
        for(auto& b:s.outputPointers)for(auto* ch:b)std::fill(ch,ch+frames,0);return;
    }
    if(outgoing)for(std::size_t i=0;i<block.outputEventCount;++i){
        auto& e=s.received[i];if(e.kind==cybridge::EventKind::parameter)continue;
        alignas(8) std::byte raw[sizeof(LV2_Atom_Event)+8]{};auto* n=reinterpret_cast<LV2_Atom_Event*>(raw);
        n->time.frames=e.frame;n->body={3,s.midi};auto* data=reinterpret_cast<std::uint8_t*>(n+1);
        std::memcpy(data,e.midi,3);if(e.kind!=cybridge::EventKind::midi){data[0]=static_cast<std::uint8_t>((e.kind==cybridge::EventKind::noteOn?0x90:0x80)|e.channel);data[1]=static_cast<std::uint8_t>(e.key);data[2]=static_cast<std::uint8_t>(e.value*127);}
        lv2_atom_sequence_append_event(outgoing,capacity,n);
    }
}
void cleanup(LV2_Handle p){auto* s=static_cast<Instance*>(p);if(s->active)s->plugin.stop();delete s;}
LV2_State_Status save(LV2_Handle p,LV2_State_Store_Function store,LV2_State_Handle handle,std::uint32_t,const LV2_Feature*const*){
    auto& s=*static_cast<Instance*>(p);if(s.active)return LV2_STATE_ERR_UNKNOWN;
    try{auto bytes=s.plugin.saveState();return store(handle,s.stateKey,bytes.data(),bytes.size(),s.chunk,LV2_STATE_IS_POD|LV2_STATE_IS_PORTABLE);}catch(...){return LV2_STATE_ERR_UNKNOWN;}
}
LV2_State_Status restore(LV2_Handle p,LV2_State_Retrieve_Function retrieve,LV2_State_Handle handle,std::uint32_t,const LV2_Feature*const*){
    auto& s=*static_cast<Instance*>(p);if(s.active)return LV2_STATE_ERR_UNKNOWN;
    std::size_t size=0;std::uint32_t type=0,flags=0;auto* data=retrieve(handle,s.stateKey,&size,&type,&flags);
    if(!data)return LV2_STATE_SUCCESS;if(type!=s.chunk||size>64u*1024u*1024u)return LV2_STATE_ERR_BAD_TYPE;
    try{s.plugin.loadState({static_cast<const std::byte*>(data),size});return LV2_STATE_SUCCESS;}catch(...){return LV2_STATE_ERR_UNKNOWN;}
}
const void* extension(const char* uri){static const LV2_State_Interface state{save,restore};return !std::strcmp(uri,LV2_STATE__interface)?&state:nullptr;}
const LV2_Descriptor descriptor{CYBRIDGE_PLUGIN_URI,instantiate,connect,activate,run,deactivate,cleanup,extension};
}
extern "C" LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(std::uint32_t index){return index?nullptr:&descriptor;}
