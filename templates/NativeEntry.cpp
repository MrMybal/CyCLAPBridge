// SPDX-License-Identifier: MIT
#include <clap/clap.h>
#include <cyclapbridge/Plugin.h>
#include "NativeConfig.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <array>
namespace {
std::filesystem::path payloadRoot;
const char* features[]{CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,nullptr};
const clap_plugin_descriptor_t descriptor{CLAP_VERSION,CYBRIDGE_PLUGIN_ID,CYBRIDGE_PLUGIN_NAME,
    "CyCLAPBridge","","","","0.2.0","Native format hosted by CyCLAPBridge",features};
struct Instance {
    clap_plugin_t clap{};const clap_host_t* host;
    std::unique_ptr<cybridge::HostedPlugin> native;
    std::vector<cybridge::AudioBus> inputs,outputs;
    std::vector<cybridge::Event> events,received;
    std::unique_ptr<std::atomic<double>[]> values;
    std::unique_ptr<std::atomic<bool>[]> dirty;
    bool active=false,processing=false;
#include "NativeEditor.inc"
    explicit Instance(const clap_host_t* h):host(h){}
    static Instance& self(const clap_plugin_t* p){return *static_cast<Instance*>(p->plugin_data);}
    static bool CLAP_ABI init(const clap_plugin_t* p) {
        auto& s=self(p);
        try {
            s.native=std::make_unique<cybridge::HostedPlugin>(cybridge::parseFormat(CYBRIDGE_SOURCE_FORMAT),
                payloadRoot/std::filesystem::u8path(CYBRIDGE_SOURCE_FILE),CYBRIDGE_SOURCE_ID);
            auto& params=s.native->parameters();s.values=std::make_unique<std::atomic<double>[]>(params.size());
            s.dirty=std::make_unique<std::atomic<bool>[]>(params.size());
            for(std::size_t i=0;i<params.size();++i){s.values[i].store(params[i].defaultValue);s.dirty[i].store(false);}
            s.inputs.resize(s.native->inputs().size());s.outputs.resize(s.native->outputs().size());
            return true;
        }catch(...){return false;}
    }
    static void CLAP_ABI destroy(const clap_plugin_t* p){guiDestroy(p);delete &self(p);}
    static bool CLAP_ABI activate(const clap_plugin_t* p,double rate,std::uint32_t,std::uint32_t frames){
        auto& s=self(p);try{s.events.resize(4096);s.received.resize(4096);s.native->prepare(rate,frames,4096);s.active=true;return true;}catch(...){return false;}
    }
    static void CLAP_ABI deactivate(const clap_plugin_t* p){auto& s=self(p);try{s.native->release();s.active=false;}catch(...){}}
    static bool CLAP_ABI start(const clap_plugin_t* p){auto& s=self(p);return s.processing=s.native->start();}
    static void CLAP_ABI stop(const clap_plugin_t* p){auto& s=self(p);s.native->stop();s.processing=false;}
    static void CLAP_ABI reset(const clap_plugin_t*){}
    static bool readEvent(const clap_event_header_t* h,cybridge::Event& e) {
        if(!h||h->space_id!=CLAP_CORE_EVENT_SPACE_ID)return false;
        e={};e.frame=h->time;
        if(h->type==CLAP_EVENT_PARAM_VALUE&&h->size>=sizeof(clap_event_param_value_t)){
            auto& p=*reinterpret_cast<const clap_event_param_value_t*>(h);
            if(p.note_id!=-1||p.port_index!=-1||p.channel!=-1||p.key!=-1)return false;
            e.parameterId=p.param_id;e.value=p.value;return true;
        }
        if(h->type==CLAP_EVENT_MIDI&&h->size>=sizeof(clap_event_midi_t)){
            auto& p=*reinterpret_cast<const clap_event_midi_t*>(h);e.kind=cybridge::EventKind::midi;e.port=p.port_index;std::memcpy(e.midi,p.data,3);return true;
        }
        if((h->type==CLAP_EVENT_NOTE_ON||h->type==CLAP_EVENT_NOTE_OFF)&&h->size>=sizeof(clap_event_note_t)){
            auto& p=*reinterpret_cast<const clap_event_note_t*>(h);e.kind=h->type==CLAP_EVENT_NOTE_ON?cybridge::EventKind::noteOn:cybridge::EventKind::noteOff;
            e.port=p.port_index;e.channel=p.channel;e.key=p.key;e.noteId=p.note_id;e.value=p.velocity;return true;
        }
        return false;
    }
    static clap_process_status CLAP_ABI process(const clap_plugin_t* p,const clap_process_t* b) {
        auto& s=self(p);
        if(b->audio_inputs_count!=s.inputs.size()||b->audio_outputs_count!=s.outputs.size())return CLAP_PROCESS_ERROR;
        for(std::size_t i=0;i<s.inputs.size();++i)s.inputs[i]={b->audio_inputs[i].data32,b->audio_inputs[i].channel_count};
        for(std::size_t i=0;i<s.outputs.size();++i)s.outputs[i]={b->audio_outputs[i].data32,b->audio_outputs[i].channel_count};
        std::uint32_t count=0;
        for(std::size_t n=0;n<s.native->parameters().size();++n)if(s.dirty[n].exchange(false)){
            if(count>=s.events.size())return CLAP_PROCESS_ERROR;
            auto& e=s.events[count++];e={};e.parameterId=s.native->parameters()[n].id;e.value=s.values[n].load();
        }
        auto incoming=b->in_events?b->in_events->size(b->in_events):0;
        if(incoming>s.events.size()-count)return CLAP_PROCESS_ERROR;
        for(std::uint32_t i=0;i<incoming;++i) {
            auto& event=s.events[count++];
            if(!readEvent(b->in_events->get(b->in_events,i),event))return CLAP_PROCESS_ERROR;
            if(event.kind==cybridge::EventKind::parameter)
                for(std::size_t n=0;n<s.native->parameters().size();++n)if(s.native->parameters()[n].id==event.parameterId)s.values[n].store(event.value,std::memory_order_relaxed);
        }
        cybridge::ProcessBlock block{b->frames_count,s.inputs,s.outputs,std::span(s.events.data(),count),s.received};
        if(s.native->process(block)!=cybridge::ProcessStatus::ok)return CLAP_PROCESS_ERROR;
        s.outputUiEdits(b->out_events);
        if(b->out_events)for(std::size_t i=0;i<block.outputEventCount;++i) {
            auto& e=s.received[i];
            if(e.kind==cybridge::EventKind::parameter)for(std::size_t n=0;n<s.native->parameters().size();++n)if(s.native->parameters()[n].id==e.parameterId){s.values[n].store(e.value);break;}
            if(e.kind==cybridge::EventKind::parameter){clap_event_param_value_t n{};n.header={sizeof(n),e.frame,CLAP_CORE_EVENT_SPACE_ID,CLAP_EVENT_PARAM_VALUE,0};n.param_id=e.parameterId;n.value=e.value;n.note_id=n.port_index=n.channel=n.key=-1;b->out_events->try_push(b->out_events,&n.header);}
            else if(e.kind==cybridge::EventKind::midi){clap_event_midi_t n{};n.header={sizeof(n),e.frame,CLAP_CORE_EVENT_SPACE_ID,CLAP_EVENT_MIDI,0};n.port_index=e.port;std::memcpy(n.data,e.midi,3);b->out_events->try_push(b->out_events,&n.header);}
            else if(e.kind==cybridge::EventKind::gestureBegin||e.kind==cybridge::EventKind::gestureEnd){clap_event_param_gesture_t n{};n.header={sizeof(n),e.frame,CLAP_CORE_EVENT_SPACE_ID,static_cast<std::uint16_t>(e.kind==cybridge::EventKind::gestureBegin?CLAP_EVENT_PARAM_GESTURE_BEGIN:CLAP_EVENT_PARAM_GESTURE_END),0};n.param_id=e.parameterId;b->out_events->try_push(b->out_events,&n.header);}
            else{clap_event_note_t n{};n.header={sizeof(n),e.frame,CLAP_CORE_EVENT_SPACE_ID,static_cast<std::uint16_t>(e.kind==cybridge::EventKind::noteOn?CLAP_EVENT_NOTE_ON:CLAP_EVENT_NOTE_OFF),0};n.note_id=e.noteId;n.port_index=e.port;n.channel=e.channel;n.key=e.key;n.velocity=e.value;b->out_events->try_push(b->out_events,&n.header);}
        }
        return CLAP_PROCESS_CONTINUE;
    }
    static std::uint32_t CLAP_ABI audioCount(const clap_plugin_t* p,bool in){return static_cast<std::uint32_t>(in?self(p).native->inputs().size():self(p).native->outputs().size());}
    static bool CLAP_ABI audioGet(const clap_plugin_t* p,std::uint32_t index,bool in,clap_audio_port_info_t* info){
        auto& ports=in?self(p).native->inputs():self(p).native->outputs();if(index>=ports.size())return false;
        *info={};info->id=ports[index].id;info->channel_count=ports[index].channels;info->in_place_pair=CLAP_INVALID_ID;
        std::snprintf(info->name,sizeof(info->name),"%s",ports[index].name.c_str());info->flags=index?0:CLAP_AUDIO_PORT_IS_MAIN;
        info->port_type=info->channel_count==1?CLAP_PORT_MONO:info->channel_count==2?CLAP_PORT_STEREO:nullptr;return true;
    }
    static std::uint32_t CLAP_ABI paramCount(const clap_plugin_t* p){return static_cast<std::uint32_t>(self(p).native->parameters().size());}
    static bool CLAP_ABI paramInfo(const clap_plugin_t* p,std::uint32_t index,clap_param_info_t* out){
        auto& list=self(p).native->parameters();if(index>=list.size())return false;const auto& info=list[index];
        *out={};out->id=info.id;out->min_value=info.minimum;out->max_value=info.maximum;out->default_value=info.defaultValue;
        out->flags=(info.automatable?CLAP_PARAM_IS_AUTOMATABLE:0)|(info.stepped?CLAP_PARAM_IS_STEPPED:0);
        std::snprintf(out->name,sizeof(out->name),"%s",info.name.c_str());return true;
    }
    static bool CLAP_ABI paramValue(const clap_plugin_t* p,clap_id id,double* out){
        auto& s=self(p);for(std::size_t i=0;i<s.native->parameters().size();++i)if(s.native->parameters()[i].id==id){*out=s.values[i].load(std::memory_order_relaxed);return true;}return false;
    }
    static bool CLAP_ABI toText(const clap_plugin_t*,clap_id,double value,char* text,std::uint32_t size){return size&&std::snprintf(text,size,"%.9g",value)>0;}
    static bool CLAP_ABI fromText(const clap_plugin_t*,clap_id,const char* text,double* value){return std::sscanf(text,"%lf",value)==1;}
    static void CLAP_ABI flush(const clap_plugin_t* p,const clap_input_events_t* in,const clap_output_events_t* out){
        auto& s=self(p);s.outputUiEdits(out);if(!in)return;
        for(std::uint32_t i=0;i<in->size(in);++i){
            cybridge::Event e;if(!readEvent(in->get(in,i),e)||e.kind!=cybridge::EventKind::parameter)continue;
            for(std::size_t n=0;n<s.native->parameters().size();++n)if(s.native->parameters()[n].id==e.parameterId){
                s.values[n].store(e.value);s.dirty[n].store(true);break;
            }
        }
    }
    static std::uint32_t CLAP_ABI noteCount(const clap_plugin_t*,bool){return 1;}
    static bool CLAP_ABI noteGet(const clap_plugin_t*,std::uint32_t i,bool,clap_note_port_info_t* n){if(i)return false;*n={};n->id=0;n->supported_dialects=CLAP_NOTE_DIALECT_CLAP|CLAP_NOTE_DIALECT_MIDI;n->preferred_dialect=CLAP_NOTE_DIALECT_MIDI;std::strcpy(n->name,"MIDI");return true;}
    static std::uint32_t CLAP_ABI latency(const clap_plugin_t* p){return self(p).native->latencyFrames();}
    static bool CLAP_ABI save(const clap_plugin_t* p,const clap_ostream_t* out){
        try{
            auto& s=self(p);auto native=s.native->saveState();
            auto number=static_cast<std::uint32_t>(s.native->parameters().size());
            if(native.size()+12+number*16>64u*1024u*1024u)return false;
            std::vector<std::byte> bytes(12+number*16+native.size());
            std::memcpy(bytes.data(),"CYBRST03",8);std::memcpy(bytes.data()+8,&number,4);
            for(std::uint32_t i=0;i<number;++i){
                auto id=s.native->parameters()[i].id;auto value=s.values[i].load();std::uint32_t pending=s.dirty[i].load()?1:0;
                std::memcpy(bytes.data()+12+i*16,&id,4);std::memcpy(bytes.data()+16+i*16,&pending,4);std::memcpy(bytes.data()+20+i*16,&value,8);
            }
            if(!native.empty())std::memcpy(bytes.data()+12+number*16,native.data(),native.size());
            std::size_t n=0;while(n<bytes.size()){auto count=out->write(out,bytes.data()+n,bytes.size()-n);if(count<=0||static_cast<std::uint64_t>(count)>bytes.size()-n)return false;n+=static_cast<std::size_t>(count);}return true;
        }catch(...){return false;}
    }
    static bool CLAP_ABI load(const clap_plugin_t* p,const clap_istream_t* in){
        try{
            std::vector<std::byte> bytes;std::byte buffer[4096];for(;;){auto count=in->read(in,buffer,sizeof(buffer));if(count<0||count>sizeof(buffer))return false;if(!count)break;if(bytes.size()+count>64u*1024u*1024u)return false;bytes.insert(bytes.end(),buffer,buffer+count);}
            auto& s=self(p);bool v3=bytes.size()>=12&&!std::memcmp(bytes.data(),"CYBRST03",8);if(bytes.size()<12||(!v3&&std::memcmp(bytes.data(),"CYBRST02",8)))return false;auto stride=v3?16u:12u;
            std::uint32_t number;std::memcpy(&number,bytes.data()+8,4);
            if(number!=s.native->parameters().size()||number>65536||bytes.size()<12+number*stride)return false;
            for(std::uint32_t i=0;i<number;++i){
                std::uint32_t id,pending=1;double value;std::memcpy(&id,bytes.data()+12+i*stride,4);if(v3)std::memcpy(&pending,bytes.data()+16+i*stride,4);std::memcpy(&value,bytes.data()+(v3?20:16)+i*stride,8);
                auto& info=s.native->parameters()[i];
                if(pending>1||id!=info.id||!std::isfinite(value)||value<info.minimum||value>info.maximum)return false;
            }
            s.native->loadState(std::span(bytes).subspan(12+number*stride));
            for(std::uint32_t i=0;i<number;++i){double value;std::uint32_t pending=1;if(v3)std::memcpy(&pending,bytes.data()+16+i*stride,4);std::memcpy(&value,bytes.data()+(v3?20:16)+i*stride,8);if(!pending)s.native->parameterValue(s.native->parameters()[i].id,value);s.values[i].store(value);s.dirty[i].store(pending!=0);}
            return true;
        }catch(...){return false;}
    }
    static const void* CLAP_ABI extension(const clap_plugin_t*,const char* id){
        static const clap_plugin_audio_ports_t audio{audioCount,audioGet};
        static const clap_plugin_params_t params{paramCount,paramInfo,paramValue,toText,fromText,flush};
        static const clap_plugin_note_ports_t notes{noteCount,noteGet};
        static const clap_plugin_latency_t delay{latency};
        static const clap_plugin_state_t state{save,load};
        static const clap_plugin_gui_t gui{guiSupported,guiPreferred,guiCreate,guiDestroy,guiScale,guiSize,guiCanResize,guiHints,guiAdjust,guiSetSize,guiParent,guiTransient,guiTitle,guiShow,guiHide};
        static const clap_plugin_timer_support_t timers{timer};
        if(!std::strcmp(id,CLAP_EXT_GUI))return &gui;
        if(!std::strcmp(id,CLAP_EXT_TIMER_SUPPORT))return &timers;
        if(!std::strcmp(id,CLAP_EXT_AUDIO_PORTS))return &audio;if(!std::strcmp(id,CLAP_EXT_PARAMS))return &params;
        if(!std::strcmp(id,CLAP_EXT_NOTE_PORTS))return &notes;if(!std::strcmp(id,CLAP_EXT_LATENCY))return &delay;
        if(!std::strcmp(id,CLAP_EXT_STATE))return &state;return nullptr;
    }
    static void CLAP_ABI mainThread(const clap_plugin_t* p){auto& s=self(p);try{auto r=s.native->pumpMainThread();if(r&cybridge::restartRequested)s.host->request_restart(s.host);
        if(r&(cybridge::flushRequested|cybridge::processRequested))s.host->request_process(s.host);
        if(s.uiOverflow.exchange(false)){auto* params=static_cast<const clap_host_params_t*>(s.host->get_extension(s.host,CLAP_EXT_PARAMS));if(params&&params->rescan)params->rescan(s.host,CLAP_PARAM_RESCAN_VALUES);}
    }catch(...) {}}
};
bool CLAP_ABI entryInit(const char* path){
    try{auto p=std::filesystem::u8path(path);
#ifdef __APPLE__
        if(!std::filesystem::is_directory(p)&&p.parent_path().filename()=="MacOS")p=p.parent_path().parent_path().parent_path();
        if(std::filesystem::is_directory(p))payloadRoot=p/"Contents/Resources";else payloadRoot=p.parent_path();
#else
        payloadRoot=p.parent_path();
#endif
        return true;
    }catch(...){return false;}
}
void CLAP_ABI entryDeinit(){}
std::uint32_t CLAP_ABI count(const clap_plugin_factory_t*){return 1;}
const clap_plugin_descriptor_t* CLAP_ABI get(const clap_plugin_factory_t*,std::uint32_t n){return n?nullptr:&descriptor;}
const clap_plugin_t* CLAP_ABI create(const clap_plugin_factory_t*,const clap_host_t* host,const char* id){
    if(std::strcmp(id,descriptor.id))return nullptr;
    try{auto* s=new Instance(host);s->clap={&descriptor,s,Instance::init,Instance::destroy,Instance::activate,Instance::deactivate,Instance::start,Instance::stop,Instance::reset,Instance::process,Instance::extension,Instance::mainThread};return &s->clap;}catch(...){return nullptr;}
}
const void* CLAP_ABI factory(const char* id){static const clap_plugin_factory_t f{count,get,create};return !std::strcmp(id,CLAP_PLUGIN_FACTORY_ID)?&f:nullptr;}
}
extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry{CLAP_VERSION,entryInit,entryDeinit,factory};
