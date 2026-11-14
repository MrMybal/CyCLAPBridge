// SPDX-License-Identifier: MIT
#include "HostedBackend.h"
#include "Process.h"
#include <lilv/lilv.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/buf-size/buf-size.h>
#include <lv2/midi/midi.h>
#include <lv2/state/state.h>
#include <lv2/urid/urid.h>
#include <lv2/ui/ui.h>
#include <lv2/instance-access/instance-access.h>
#include <lv2/data-access/data-access.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <algorithm>
#include <cstring>
#include <cstdlib>
namespace cybridge::detail {
namespace {
struct Node {
    LilvNode* p;
    ~Node(){lilv_node_free(p);}
    operator const LilvNode*() const {return p;}
};
struct Lv2 final : HostedBackend {
    LilvWorld* world=lilv_world_new();
    const LilvPlugin* plugin=nullptr;
    LilvInstance* instance=nullptr;
    enum Kind {audio,controlPort,atom};
    struct Port {
        Kind kind; bool input; std::uint32_t index,channel=0;
        std::string symbol; float value=0;
        std::vector<std::uint64_t> storage;
        LV2_Atom_Sequence* sequence(){return reinterpret_cast<LV2_Atom_Sequence*>(storage.data());}
    };
    std::vector<Port> ports;
    std::vector<std::string> uris;
    std::vector<std::byte> pendingState;
    LV2_URID_Map mapper{this,map};
    LV2_URID_Unmap unmapper{this,unmap};
    LV2_Feature mapFeature{LV2_URID__map,&mapper},unmapFeature{LV2_URID__unmap,&unmapper},
        boundFeature{LV2_BUF_SIZE__boundedBlockLength,nullptr};
    const LV2_Feature* features[4]{&mapFeature,&unmapFeature,&boundFeature,nullptr};
    LV2_URID midiType=0,sequenceType=0,floatType=0,chunkType=0;
#ifdef _WIN32
    LilvUIs* uis=nullptr;const LilvUI* selectedUi=nullptr;
    HMODULE uiModule=nullptr;const LV2UI_Descriptor* uiDescriptor=nullptr;
    LV2UI_Handle ui=nullptr;HWND widget=nullptr;NativeWindow uiParent{WindowApi::win32,nullptr};
    bool editorCreated=false,uiVisible=false;EditorSize uiSize{};
    const LV2UI_Idle_Interface* uiIdle=nullptr;const LV2UI_Resize* uiResize=nullptr;
    LV2UI_Resize resizeFeature{this,requestUiResize};
    LV2UI_Touch touchFeature{this,touch};
    LV2UI_Port_Map portMap{this,portIndex};
    LV2_Extension_Data_Feature access{};
    std::vector<LV2_Feature> uiFeatures;std::vector<const LV2_Feature*> uiFeaturePointers;
    static int requestUiResize(void* handle,int w,int h){
        auto& s=*static_cast<Lv2*>(handle);if(w<=0||h<=0)return 1;EditorSize size{static_cast<unsigned>(w),static_cast<unsigned>(h)};
        if(!validSize(size)||!s.editorCallbacks.requestResize||!s.editorCallbacks.requestResize(s.editorCallbacks.context,size))return 1;
        s.uiSize=size;return 0;
    }
    static void touch(void* handle,std::uint32_t id,bool grabbed){auto& s=*static_cast<Lv2*>(handle);for(std::size_t i=0;i<s.params.size();++i)if(s.params[i].id==id)s.edited(id,s.mirror[i].value.load(),grabbed?EditGesture::begin:EditGesture::end);}
    static std::uint32_t portIndex(void* handle,const char* symbol){auto& s=*static_cast<Lv2*>(handle);for(auto& p:s.ports)if(p.symbol==symbol)return p.index;return LV2UI_INVALID_PORT_INDEX;}
    static void uiWrite(void* handle,std::uint32_t id,std::uint32_t size,std::uint32_t format,const void* bytes){
        auto& s=*static_cast<Lv2*>(handle);if(format||size!=sizeof(float)||!bytes)return;float v;std::memcpy(&v,bytes,4);s.edited(id,v,EditGesture::value);
    }
    const LilvUI* findUi() const {
        if(!uis)return nullptr;Node type{lilv_new_uri(world,LV2_UI__WindowsUI)};
        LILV_FOREACH(uis,i,uis){auto* candidate=lilv_uis_get(uis,i);if(!lilv_ui_is_a(candidate,type))continue;
            Node requiredFeature{lilv_new_uri(world,LV2_CORE__requiredFeature)};
            auto* nodes=lilv_world_find_nodes(world,lilv_ui_get_uri(candidate),requiredFeature,nullptr);bool supported=true;
            LILV_FOREACH(nodes,n,nodes){auto uri=std::string(lilv_node_as_uri(lilv_nodes_get(nodes,n)));
                if(uri!=LV2_UI__parent&&uri!=LV2_UI__resize&&uri!=LV2_UI__touch&&uri!=LV2_UI__portMap&&uri!=LV2_UI__idleInterface&&uri!=LV2_URID__map&&uri!=LV2_URID__unmap&&uri!=LV2_INSTANCE_ACCESS_URI&&uri!=LV2_DATA_ACCESS_URI)supported=false;
            }lilv_nodes_free(nodes);if(supported)return candidate;
        }return nullptr;
    }
    bool editorSupported(WindowApi a) const override{return a==WindowApi::win32&&findUi();}
    bool createEditor(WindowApi a,const EditorCallbacks& c) override{if(editorCreated||!editorSupported(a))return false;selectedUi=findUi();editorCallbacks=c;editorCreated=true;uiSize={};return true;}
    void destroyEditor() override{
        if(ui&&uiDescriptor&&uiDescriptor->cleanup)uiDescriptor->cleanup(ui);ui=nullptr;widget=nullptr;uiDescriptor=nullptr;uiIdle=nullptr;uiResize=nullptr;
        if(uiModule)FreeLibrary(uiModule);uiModule=nullptr;editorCreated=false;selectedUi=nullptr;uiParent.handle=nullptr;editorCallbacks={};
    }
    bool setEditorParent(NativeWindow parent) override{
        if(!editorCreated||ui||parent.api!=WindowApi::win32||!parent.handle)return false;
        if(!instance){instance=lilv_plugin_instantiate(plugin,48000,features);if(!instance)return false;for(auto& p:ports)if(p.kind==controlPort)lilv_instance_connect_port(instance,p.index,&p.value);}
        char* native=lilv_file_uri_parse(lilv_node_as_uri(lilv_ui_get_binary_uri(selectedUi)),nullptr);if(!native)return false;
        auto path=std::filesystem::u8path(native);lilv_free(native);uiModule=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);if(!uiModule)return false;
        auto entry=reinterpret_cast<const LV2UI_Descriptor*(*)(std::uint32_t)>(GetProcAddress(uiModule,"lv2ui_descriptor"));
        if(entry)for(std::uint32_t i=0;i<1024;++i){auto* candidate=entry(i);if(!candidate)break;if(candidate->URI&&std::string(candidate->URI)==lilv_node_as_uri(lilv_ui_get_uri(selectedUi))){uiDescriptor=candidate;break;}}
        if(!uiDescriptor||!uiDescriptor->instantiate||!uiDescriptor->cleanup){FreeLibrary(uiModule);uiModule=nullptr;return false;}
        access.data_access=lilv_instance_get_descriptor(instance)->extension_data;
        uiFeatures={{LV2_UI__parent,parent.handle},{LV2_UI__resize,&resizeFeature},{LV2_UI__touch,&touchFeature},{LV2_UI__portMap,&portMap},{LV2_UI__idleInterface,nullptr},mapFeature,unmapFeature,{LV2_INSTANCE_ACCESS_URI,lilv_instance_get_handle(instance)},{LV2_DATA_ACCESS_URI,&access}};
        uiFeaturePointers.clear();for(auto& f:uiFeatures)uiFeaturePointers.push_back(&f);uiFeaturePointers.push_back(nullptr);
        char* bundlePath=lilv_file_uri_parse(lilv_node_as_uri(lilv_ui_get_bundle_uri(selectedUi)),nullptr);if(!bundlePath){FreeLibrary(uiModule);uiModule=nullptr;uiDescriptor=nullptr;return false;}
        LV2UI_Widget handle=nullptr;ui=uiDescriptor->instantiate(uiDescriptor,info.id.c_str(),bundlePath,uiWrite,this,&handle,uiFeaturePointers.data());lilv_free(bundlePath);
        if(!ui){FreeLibrary(uiModule);uiModule=nullptr;uiDescriptor=nullptr;return false;}
        widget=static_cast<HWND>(handle);
        if(!IsWindow(widget)||!IsChild(static_cast<HWND>(parent.handle),widget)){uiDescriptor->cleanup(ui);ui=nullptr;widget=nullptr;FreeLibrary(uiModule);uiModule=nullptr;uiDescriptor=nullptr;return false;}
        uiParent=parent;
        if(uiDescriptor->extension_data){uiIdle=static_cast<const LV2UI_Idle_Interface*>(uiDescriptor->extension_data(LV2_UI__idleInterface));uiResize=static_cast<const LV2UI_Resize*>(uiDescriptor->extension_data(LV2_UI__resize));}
        if(widget){RECT rect{};if(GetClientRect(widget,&rect)&&rect.right>0&&rect.bottom>0)uiSize={static_cast<unsigned>(rect.right),static_cast<unsigned>(rect.bottom)};ShowWindow(widget,SW_HIDE);}
        for(std::size_t i=0;i<params.size();++i)editorParameter(params[i].id,mirror[i].value.load());return true;
    }
    EditorSize editorSize() const override{return uiSize;}
    bool editorCanResize() const override{return uiResize&&uiResize->ui_resize;}
    bool adjustEditorSize(EditorSize& size) const override{return editorCanResize()&&validSize(size);}
    bool resizeEditor(EditorSize size) override{if(!editorCanResize()||uiResize->ui_resize(ui,size.width,size.height))return false;uiSize=size;return true;}
    bool showEditor(bool show) override{if(!widget)return false;ShowWindow(widget,show?SW_SHOWNA:SW_HIDE);uiVisible=show;return true;}
    void editorParameter(std::uint32_t id,double value) override{float v=static_cast<float>(value);if(ui&&uiDescriptor->port_event)uiDescriptor->port_event(ui,id,4,0,&v);}
    std::uint32_t pump() override{control();if(ui&&uiVisible&&uiIdle&&uiIdle->idle&&uiIdle->idle(ui)){if(editorCallbacks.closed)editorCallbacks.closed(editorCallbacks.context);uiVisible=false;}return 0;}
#endif
    static LV2_URID map(void* handle,const char* uri) {
        auto& s=*static_cast<Lv2*>(handle);
        for(std::size_t i=0;i<s.uris.size();++i) if(s.uris[i]==uri) return static_cast<LV2_URID>(i+1);
        // Unknown URIDs are preparation/control work, never an audio allocation.
        if(s.running || s.uris.size()>=65536) return 0;
        s.uris.emplace_back(uri); return static_cast<LV2_URID>(s.uris.size());
    }
    static const char* unmap(void* handle,LV2_URID id) {
        auto& s=*static_cast<Lv2*>(handle);
        return id && id<=s.uris.size()?s.uris[id-1].c_str():nullptr;
    }
    explicit Lv2(const std::filesystem::path& bundle,const std::string& id) {
        if(!world) throw std::bad_alloc();
        try {
            auto p=utf8(std::filesystem::canonical(bundle))+"/";
            Node uri{lilv_new_file_uri(world,nullptr,p.c_str())};
            lilv_world_load_bundle(world,uri);
            auto* all=lilv_world_get_all_plugins(world);
            LILV_FOREACH(plugins,i,all) {
                auto* candidate=lilv_plugins_get(all,i);
                if(id.empty() || id==lilv_node_as_uri(lilv_plugin_get_uri(candidate))){plugin=candidate;break;}
            }
            if(!plugin) throw std::runtime_error("LV2 URI was not found in selected bundle");
            auto* required=lilv_plugin_get_required_features(plugin);
            bool unsupported=false;
            LILV_FOREACH(nodes,i,required) {
                auto value=std::string(lilv_node_as_uri(lilv_nodes_get(required,i)));
                if(value!=LV2_URID__map && value!=LV2_URID__unmap && value!=LV2_BUF_SIZE__boundedBlockLength) unsupported=true;
            }
            lilv_nodes_free(required);
            if(unsupported) throw std::runtime_error("LV2 requires a host feature not provided by this backend");
            Node name{lilv_plugin_get_name(plugin)};
            info={lilv_node_as_uri(lilv_plugin_get_uri(plugin)),lilv_node_as_string(name),"LV2","1"};
            Node in{lilv_new_uri(world,LV2_CORE__InputPort)},au{lilv_new_uri(world,LV2_CORE__AudioPort)},
                ctl{lilv_new_uri(world,LV2_CORE__ControlPort)},at{lilv_new_uri(world,LV2_ATOM__AtomPort)},
                integer{lilv_new_uri(world,LV2_CORE__integer)};
            auto count=lilv_plugin_get_num_ports(plugin); if(count>8192) throw std::runtime_error("LV2 port limit");
            std::uint32_t ni=0,no=0;
            for(std::uint32_t i=0;i<count;++i) {
                auto* port=lilv_plugin_get_port_by_index(plugin,i);
                bool input=lilv_port_is_a(plugin,port,in);
                Port p{}; p.input=input; p.index=i; p.symbol=lilv_node_as_string(lilv_port_get_symbol(plugin,port));
                Node portName{lilv_port_get_name(plugin,port)};
                if(lilv_port_is_a(plugin,port,au)){p.kind=audio;p.channel=input?ni++:no++;}
                else if(lilv_port_is_a(plugin,port,ctl)) {
                    p.kind=controlPort;
                    LilvNode *def=nullptr,*lo=nullptr,*hi=nullptr; lilv_port_get_range(plugin,port,&def,&lo,&hi);
                    auto value=[](LilvNode* n,double fallback){return n && (lilv_node_is_float(n)||lilv_node_is_int(n))?lilv_node_as_float(n):fallback;};
                    p.value=static_cast<float>(value(def,0));
                    if(input)params.push_back({i,lilv_node_as_string(portName),value(lo,0),value(hi,1),p.value,true,bool(lilv_port_has_property(plugin,port,integer))});
                    lilv_node_free(def);lilv_node_free(lo);lilv_node_free(hi);
                } else if(lilv_port_is_a(plugin,port,at)){p.kind=atom;}
                else throw std::runtime_error("Unsupported required LV2 port type");
                ports.push_back(std::move(p));
            }
            if(ni)ins.push_back({0,ni,"LV2 inputs"}); if(no)outs.push_back({0,no,"LV2 outputs"});
            midiType=map(this,LV2_MIDI__MidiEvent);sequenceType=map(this,LV2_ATOM__Sequence);
            floatType=map(this,LV2_ATOM__Float);chunkType=map(this,LV2_ATOM__Chunk);
#ifdef _WIN32
            uis=lilv_plugin_get_uis(plugin);
#endif
        } catch(...) {lilv_world_free(world);world=nullptr;throw;}
    }
    ~Lv2() override {
#ifdef _WIN32
        destroyEditor();if(uis)lilv_uis_free(uis);
#endif
        if(instance){if(running)lilv_instance_deactivate(instance);lilv_instance_free(instance);} if(world)lilv_world_free(world); }
    void prepare(double rate,std::uint32_t f,std::uint32_t e) override {
        limits(rate,f,e);
#ifdef _WIN32
        auto reopen=editorCreated;auto parent=uiParent;auto callbacks=editorCallbacks;auto visible=uiVisible;
        if(reopen)destroyEditor();
#endif
        if(instance){
            if(pendingState.empty())pendingState=save(64u*1024u*1024u);
            lilv_instance_free(instance);instance=nullptr;
        }
        for(auto& p:ports)if(p.kind==atom)p.storage.resize(std::max<std::size_t>(512,4+static_cast<std::size_t>(e)*4));
        instance=lilv_plugin_instantiate(plugin,rate,features);
        if(!instance)throw std::runtime_error("LV2 instantiation failed");
        for(auto& p:ports) {
            if(p.kind==controlPort)lilv_instance_connect_port(instance,p.index,&p.value);
            else if(p.kind==atom)lilv_instance_connect_port(instance,p.index,p.storage.data());
        }
        prepared=true;
        if(!pendingState.empty()){auto pending=std::move(pendingState);pendingState.clear();load(pending);}
#ifdef _WIN32
        if(reopen){if(!createEditor(WindowApi::win32,callbacks)||(parent.handle&&!setEditorParent(parent)))throw std::runtime_error("LV2 editor recreation failed");if(parent.handle)showEditor(visible);}
#endif
    }
    bool start() noexcept override {if(!prepared||running)return false;lilv_instance_activate(instance);running=true;return true;}
    void stop() noexcept override {if(running)lilv_instance_deactivate(instance);running=false;}
    void unprepare() override {idle();prepared=false;}
    ProcessStatus process(ProcessBlock& b) noexcept override {
        if(!valid(b))return running?ProcessStatus::invalidBlock:ProcessStatus::notPrepared;
        for(const auto& e:b.events)if(e.kind!=EventKind::parameter && e.port!=0)return ProcessStatus::invalidBlock;
        for(std::uint32_t begin=0;begin<b.frames;) {
            std::uint32_t end=b.frames;
            for(const auto& e:b.events)if(e.kind==EventKind::parameter&&e.frame==begin)
                for(auto& p:ports)if(p.index==e.parameterId)p.value=static_cast<float>(e.value);
            // Find the next automation boundary independently of matching port IDs.
            for(const auto& e:b.events)if(e.kind==EventKind::parameter&&e.frame>begin)end=std::min(end,e.frame);
            for(auto& p:ports) {
                if(p.kind==audio)lilv_instance_connect_port(instance,p.index,(p.input?b.inputs[0]:b.outputs[0]).channels[p.channel]+begin);
                else if(p.kind==atom) {
                    auto* seq=p.sequence();seq->body={};
                    seq->atom={p.input?static_cast<std::uint32_t>(sizeof(LV2_Atom_Sequence_Body)):
                        static_cast<std::uint32_t>(p.storage.size()*8-sizeof(LV2_Atom)),p.input?sequenceType:chunkType};
                    if(p.input)for(const auto& e:b.events)if(e.kind!=EventKind::parameter&&e.frame>=begin&&e.frame<end) {
                        alignas(8) std::byte raw[sizeof(LV2_Atom_Event)+8]{};
                        auto* ev=reinterpret_cast<LV2_Atom_Event*>(raw);
                        ev->time.frames=e.frame-begin;ev->body={3,midiType};
                        auto* data=reinterpret_cast<std::uint8_t*>(ev+1);std::memcpy(data,e.midi,3);
                        if(e.kind!=EventKind::midi){data[0]=static_cast<std::uint8_t>((e.kind==EventKind::noteOn?0x90:0x80)|e.channel);data[1]=static_cast<std::uint8_t>(e.key);data[2]=static_cast<std::uint8_t>(e.value*127);}
                        if(!lv2_atom_sequence_append_event(seq,static_cast<std::uint32_t>(p.storage.size()*8-sizeof(LV2_Atom)),ev))b.outputEventsDropped=true;
                    }
                }
            }
            lilv_instance_run(instance,end-begin);
            for(auto& p:ports)if(p.kind==atom&&!p.input) {
                auto* seq=p.sequence();
                if(seq->atom.type!=sequenceType || seq->atom.size<sizeof(seq->body) ||
                    seq->atom.size>p.storage.size()*8-sizeof(LV2_Atom)){b.outputEventsDropped=true;continue;}
                auto* cursor=reinterpret_cast<const std::byte*>(&seq->body+1);
                auto* finish=reinterpret_cast<const std::byte*>(&seq->atom+1)+seq->atom.size;
                while(finish-cursor>=sizeof(LV2_Atom_Event)) {
                    auto* ev=reinterpret_cast<const LV2_Atom_Event*>(cursor);
                    auto total=sizeof(LV2_Atom_Event)+static_cast<std::uint64_t>(lv2_atom_pad_size(ev->body.size));
                    if(total>static_cast<std::uint64_t>(finish-cursor)){b.outputEventsDropped=true;break;}
                    if(ev->body.type==midiType&&ev->body.size==3&&ev->time.frames>=0&&ev->time.frames<end-begin) {
                        Event e;e.kind=EventKind::midi;e.frame=begin+static_cast<std::uint32_t>(ev->time.frames);
                        std::memcpy(e.midi,ev+1,3);emit(b,e);
                    }else b.outputEventsDropped=true;
                    cursor+=total;
                }
            }
            begin=end;
        }
        return ProcessStatus::ok;
    }
    static const void* portValue(const char* symbol,void* data,std::uint32_t* size,std::uint32_t* type) {
        auto& s=*static_cast<Lv2*>(data);
        for(auto& p:s.ports)if(p.kind==controlPort&&p.symbol==symbol){*size=4;*type=s.floatType;return &p.value;}
        return nullptr;
    }
    static void setValue(const char* symbol,void* data,const void* value,std::uint32_t size,std::uint32_t type) {
        auto& s=*static_cast<Lv2*>(data);
        if(size==4&&type==s.floatType)for(auto& p:s.ports)if(p.kind==controlPort&&p.symbol==symbol)std::memcpy(&p.value,value,4);
    }
    std::vector<std::byte> save(std::size_t limit) override {
        idle();
        if(!instance){
            if(!pendingState.empty()){if(pendingState.size()>limit)throw std::runtime_error("LV2 state capacity exceeded");return pendingState;}
            instance=lilv_plugin_instantiate(plugin,48000,features);
            if(!instance)throw std::runtime_error("Cannot instantiate LV2 for inactive state");
            for(auto& p:ports)if(p.kind==controlPort)lilv_instance_connect_port(instance,p.index,&p.value);
        }
        auto* state=lilv_state_new_from_instance(plugin,instance,&mapper,nullptr,nullptr,nullptr,nullptr,portValue,this,LV2_STATE_IS_POD|LV2_STATE_IS_PORTABLE,features);
        if(!state)throw std::runtime_error("LV2 state snapshot failed");
        auto* text=lilv_state_to_string(world,&mapper,&unmapper,state,"urn:cybridge:state",nullptr);
        lilv_state_free(state);
        if(!text)throw std::runtime_error("LV2 state serialization failed");
        auto size=std::strlen(text);
        if(size>limit){std::free(text);throw std::runtime_error("LV2 state capacity exceeded");}
        std::vector<std::byte> result(size);std::memcpy(result.data(),text,size);std::free(text);return result;
    }
    void load(std::span<const std::byte> bytes) override {
        idle();
        std::string text(reinterpret_cast<const char*>(bytes.data()),bytes.size());
        auto* state=lilv_state_new_from_string(world,&mapper,text.c_str());
        if(!state)throw std::runtime_error("LV2 state parse failed");
        if(std::string(lilv_node_as_uri(lilv_state_get_plugin_uri(state)))!=info.id){lilv_state_free(state);throw std::runtime_error("LV2 state belongs to another plugin");}
        if(!instance){
            lilv_state_emit_port_values(state,setValue,this);pendingState.assign(bytes.begin(),bytes.end());lilv_state_free(state);return;
        }
        lilv_state_restore(state,instance,setValue,this,LV2_STATE_IS_POD|LV2_STATE_IS_PORTABLE,features);lilv_state_free(state);
    }
    double currentParameter(std::uint32_t id) const override{for(auto& p:ports)if(p.index==id&&p.kind==controlPort)return p.value;return 0;}
    double editorValue(std::uint32_t id) const override{
        // The raw LV2 float port is owned by audio while prepared. UI reads use
        // its published mirror, including during start/stop transitions.
        if(prepared)for(std::size_t i=0;i<params.size();++i)if(params[i].id==id)return mirror[i].value.load();
        return currentParameter(id);
    }
};
}
std::unique_ptr<HostedBackend> makeLv2(const std::filesystem::path& p,const std::string& id){return std::make_unique<Lv2>(p,id);}
}
