// SPDX-License-Identifier: MIT
// Original fixture: no devices or external plugins. Optional Win32 test editor.
#include <clap/clap.h>
#include <cmath>
#include <cstring>
#include <new>
#include <atomic>
#ifdef _WIN32
#include <windows.h>
#endif
namespace {
const char* features[]{CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,CLAP_PLUGIN_FEATURE_STEREO,nullptr};
const clap_plugin_descriptor_t descriptor{CLAP_VERSION,"org.cybridge.tests.gain","CyBridge Gain",
    "CyCLAPBridge","","","","1.0.0","Original stereo gain fixture",features};
struct Gain { clap_plugin_t plugin{}; double value=1;const clap_host_t* host=nullptr;
    std::atomic<double> displayed{1};std::atomic<bool> pending{false};
#ifdef _WIN32
    HWND editor=nullptr;bool guiCreated=false;std::uint32_t width=320,height=180;
#endif
};
Gain& self(const clap_plugin_t* p) { return *static_cast<Gain*>(p->plugin_data); }
void emitGui(const clap_output_events_t* out,double value){
    if(!out)return;clap_event_param_gesture_t gesture{};gesture.header={sizeof(gesture),0,CLAP_CORE_EVENT_SPACE_ID,CLAP_EVENT_PARAM_GESTURE_BEGIN,CLAP_EVENT_IS_LIVE};gesture.param_id=7;out->try_push(out,&gesture.header);
    clap_event_param_value_t e{};e.header={sizeof(e),0,CLAP_CORE_EVENT_SPACE_ID,CLAP_EVENT_PARAM_VALUE,CLAP_EVENT_IS_LIVE};e.param_id=7;e.value=value;e.note_id=e.port_index=e.channel=e.key=-1;out->try_push(out,&e.header);
    gesture.header.type=CLAP_EVENT_PARAM_GESTURE_END;out->try_push(out,&gesture.header);
}
bool CLAP_ABI init(const clap_plugin_t*) { return true; }
void CLAP_ABI destroy(const clap_plugin_t* p) {
#ifdef _WIN32
    if(self(p).editor)DestroyWindow(self(p).editor);
#endif
    delete &self(p); }
bool CLAP_ABI activate(const clap_plugin_t*,double,std::uint32_t,std::uint32_t) { return true; }
void CLAP_ABI deactivate(const clap_plugin_t*) {}
bool CLAP_ABI start(const clap_plugin_t*) { return true; }
void CLAP_ABI stop(const clap_plugin_t*) {}
void CLAP_ABI reset(const clap_plugin_t*) {}
clap_process_status CLAP_ABI process(const clap_plugin_t* p,const clap_process_t* block) {
    if(block->audio_inputs_count!=1 || block->audio_outputs_count!=1) return CLAP_PROCESS_ERROR;
    auto& g=self(p); std::uint32_t next=0;
    if(g.pending.exchange(false)){
        g.value=g.displayed.load();emitGui(block->out_events,g.value);
    }
    auto count=block->in_events->size(block->in_events);
    for(std::uint32_t f=0;f<block->frames_count;++f) {
        while(next<count) {
            auto* e=block->in_events->get(block->in_events,next);
            if(e->time>f) break;
            if(e->type==CLAP_EVENT_PARAM_VALUE) {
                g.value=reinterpret_cast<const clap_event_param_value_t*>(e)->value;
                g.displayed.store(g.value);
                block->out_events->try_push(block->out_events,e);
            }
            ++next;
        }
        for(std::uint32_t c=0;c<2;++c)
            block->audio_outputs[0].data32[c][f]=block->audio_inputs[0].data32[c][f]*static_cast<float>(g.value);
    }
    return CLAP_PROCESS_CONTINUE;
}
std::uint32_t CLAP_ABI audioCount(const clap_plugin_t*,bool) { return 1; }
bool CLAP_ABI audioGet(const clap_plugin_t*,std::uint32_t index,bool,clap_audio_port_info_t* p) {
    if(index) return false;
    *p={}; p->id=0; std::strcpy(p->name,"Stereo"); p->flags=CLAP_AUDIO_PORT_IS_MAIN;
    p->channel_count=2; p->port_type=CLAP_PORT_STEREO; p->in_place_pair=0; return true;
}
std::uint32_t CLAP_ABI paramCount(const clap_plugin_t*) { return 1; }
bool CLAP_ABI paramInfo(const clap_plugin_t*,std::uint32_t index,clap_param_info_t* p) {
    if(index) return false;
    *p={}; p->id=7; p->flags=CLAP_PARAM_IS_AUTOMATABLE; std::strcpy(p->name,"Gain");
    p->min_value=0; p->max_value=2; p->default_value=1; return true;
}
bool CLAP_ABI paramValue(const clap_plugin_t* p,clap_id id,double* value) { if(id!=7) return false; *value=self(p).displayed.load(); return true; }
bool CLAP_ABI toText(const clap_plugin_t*,clap_id,double,char*,std::uint32_t) { return false; }
bool CLAP_ABI fromText(const clap_plugin_t*,clap_id,const char*,double*) { return false; }
void CLAP_ABI flush(const clap_plugin_t* p,const clap_input_events_t* in,const clap_output_events_t* out) {
    auto& s=self(p);if(in)for(std::uint32_t i=0;i<in->size(in);++i){auto* h=in->get(in,i);if(h->type==CLAP_EVENT_PARAM_VALUE){s.value=reinterpret_cast<const clap_event_param_value_t*>(h)->value;s.displayed.store(s.value);}}
    if(s.pending.exchange(false)){s.value=s.displayed.load();emitGui(out,s.value);}
}
bool CLAP_ABI save(const clap_plugin_t* p,const clap_ostream_t* stream) {
    // The plugin's control value is authoritative even before its next block.
    const double value=self(p).displayed.load();return stream->write(stream,&value,sizeof(value))==sizeof(value);
}
bool CLAP_ABI load(const clap_plugin_t* p,const clap_istream_t* stream) {
    double v=0;
    if(stream->read(stream,&v,sizeof(v))!=sizeof(v) || !std::isfinite(v) || v<0 || v>2) return false;
    self(p).value=v;self(p).displayed.store(v); return true;
}
#ifdef _WIN32
bool CLAP_ABI guiSupported(const clap_plugin_t*,const char* api,bool floating){return api&&!std::strcmp(api,CLAP_WINDOW_API_WIN32)&&!floating;}
bool CLAP_ABI guiPreferred(const clap_plugin_t*,const char** api,bool* floating){*api=CLAP_WINDOW_API_WIN32;*floating=false;return true;}
bool CLAP_ABI guiCreate(const clap_plugin_t* p,const char* api,bool floating){auto& s=self(p);if(s.guiCreated||!guiSupported(p,api,floating))return false;s.guiCreated=true;return true;}
void CLAP_ABI guiDestroy(const clap_plugin_t* p){auto& s=self(p);if(s.editor)DestroyWindow(s.editor);s.editor=nullptr;s.guiCreated=false;}
bool CLAP_ABI guiScale(const clap_plugin_t*,double){return false;}
bool CLAP_ABI guiSize(const clap_plugin_t* p,std::uint32_t* w,std::uint32_t* h){*w=self(p).width;*h=self(p).height;return true;}
bool CLAP_ABI guiResizeable(const clap_plugin_t*){return true;}
bool CLAP_ABI guiHints(const clap_plugin_t*,clap_gui_resize_hints_t* hints){*hints={true,true,false,0,0};return true;}
bool CLAP_ABI guiAdjust(const clap_plugin_t*,std::uint32_t* w,std::uint32_t* h){*w=std::max(200u,*w);*h=std::max(100u,*h);return true;}
bool CLAP_ABI guiSetSize(const clap_plugin_t* p,std::uint32_t w,std::uint32_t h){auto& s=self(p);s.width=w;s.height=h;if(s.editor)MoveWindow(s.editor,0,0,w,h,TRUE);return true;}
bool CLAP_ABI guiParent(const clap_plugin_t* p,const clap_window_t* parent){auto& s=self(p);if(!s.guiCreated||s.editor||!parent||std::strcmp(parent->api,CLAP_WINDOW_API_WIN32))return false;
    s.editor=CreateWindowExW(0,L"BUTTON",L"Fixture: gain 0.5",WS_CHILD|BS_PUSHBUTTON,0,0,s.width,s.height,static_cast<HWND>(parent->win32),nullptr,GetModuleHandleW(nullptr),nullptr);
    // Store the callback in a property; the test sends BM_CLICK through a subclass.
    if(!s.editor)return false;SetPropW(s.editor,L"CyBridgeFixture",&s);
    auto previous=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(s.editor,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(+[](HWND window,UINT message,WPARAM w,LPARAM l)->LRESULT{
        auto* state=static_cast<Gain*>(GetPropW(window,L"CyBridgeFixture"));
        if(message==BM_CLICK&&state){state->displayed.store(.5);state->pending.store(true);auto* params=static_cast<const clap_host_params_t*>(state->host->get_extension(state->host,CLAP_EXT_PARAMS));if(params)params->request_flush(state->host);
            auto* gui=static_cast<const clap_host_gui_t*>(state->host->get_extension(state->host,CLAP_EXT_GUI));if(gui&&gui->request_resize&&gui->request_resize(state->host,480,240)){state->width=480;state->height=240;MoveWindow(window,0,0,480,240,TRUE);}return 0;}
        auto previous=reinterpret_cast<WNDPROC>(GetPropW(window,L"CyBridgePrevious"));return previous?CallWindowProcW(previous,window,message,w,l):DefWindowProcW(window,message,w,l);
    })));SetPropW(s.editor,L"CyBridgePrevious",reinterpret_cast<HANDLE>(previous));return true;
}
bool CLAP_ABI guiTransient(const clap_plugin_t*,const clap_window_t*){return false;}
void CLAP_ABI guiTitle(const clap_plugin_t*,const char*){}
bool CLAP_ABI guiShow(const clap_plugin_t* p){if(!self(p).editor)return false;ShowWindow(self(p).editor,SW_SHOWNA);return true;}
bool CLAP_ABI guiHide(const clap_plugin_t* p){if(!self(p).editor)return false;ShowWindow(self(p).editor,SW_HIDE);return true;}
#endif
const void* CLAP_ABI extension(const clap_plugin_t*,const char* id) {
    static const clap_plugin_audio_ports_t audio{audioCount,audioGet};
    static const clap_plugin_params_t params{paramCount,paramInfo,paramValue,toText,fromText,flush};
    static const clap_plugin_state_t state{save,load};
#ifdef _WIN32
    static const clap_plugin_gui_t gui{guiSupported,guiPreferred,guiCreate,guiDestroy,guiScale,guiSize,guiResizeable,guiHints,guiAdjust,guiSetSize,guiParent,guiTransient,guiTitle,guiShow,guiHide};
    if(!std::strcmp(id,CLAP_EXT_GUI))return &gui;
#endif
    if(!std::strcmp(id,CLAP_EXT_AUDIO_PORTS)) return &audio;
    if(!std::strcmp(id,CLAP_EXT_PARAMS)) return &params;
    if(!std::strcmp(id,CLAP_EXT_STATE)) return &state;
    return nullptr;
}
void CLAP_ABI mainThread(const clap_plugin_t*) {}
std::uint32_t CLAP_ABI count(const clap_plugin_factory_t*) { return 1; }
const clap_plugin_descriptor_t* CLAP_ABI get(const clap_plugin_factory_t*,std::uint32_t n) { return n ? nullptr : &descriptor; }
const clap_plugin_t* CLAP_ABI create(const clap_plugin_factory_t*,const clap_host_t* host,const char* id) {
    if(std::strcmp(id,descriptor.id)) return nullptr;
    auto* g=new(std::nothrow) Gain;
    if(!g) return nullptr;
    g->host=host;
    g->plugin={&descriptor,g,init,destroy,activate,deactivate,start,stop,reset,process,extension,mainThread};
    return &g->plugin;
}
bool CLAP_ABI entryInit(const char*) { return true; }
void CLAP_ABI entryDeinit() {}
const void* CLAP_ABI factory(const char* id) {
    static const clap_plugin_factory_t factory{count,get,create};
    return !std::strcmp(id,CLAP_PLUGIN_FACTORY_ID) ? &factory : nullptr;
}
}
extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry{CLAP_VERSION,entryInit,entryDeinit,factory};
