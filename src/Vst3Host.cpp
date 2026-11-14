// SPDX-License-Identifier: MIT
#include "HostedBackend.h"
#include "Process.h"
#include <pluginterfaces/base/ipluginbase.h>
#include <pluginterfaces/base/ibstream.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <pluginterfaces/vst/ivsthostapplication.h>
#include <pluginterfaces/vst/ivstevents.h>
#include <pluginterfaces/vst/ivstparameterchanges.h>
#include <pluginterfaces/vst/ivstmessage.h>
#include <pluginterfaces/gui/iplugview.h>
#include <pluginterfaces/gui/iplugviewcontentscalesupport.h>
#include <algorithm>
#include <atomic>
#include <array>
#include <cstring>
#include <limits>
#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#else
#include <dlfcn.h>
#endif
namespace cybridge::detail {
namespace {
using namespace Steinberg;
using namespace Steinberg::Vst;
bool same(const TUID a,const FUID& b){return !std::memcmp(a,b,16);}
std::string utf16(const TChar* input) {
    std::string out;
    for(std::size_t i=0;input[i];++i) {
        std::uint32_t c=input[i];
        if(c>=0xd800&&c<=0xdbff&&input[i+1]>=0xdc00&&input[i+1]<=0xdfff)c=0x10000+((c-0xd800)<<10)+(input[++i]-0xdc00);
        if(c<128)out+=static_cast<char>(c);
        else if(c<2048){out+=static_cast<char>(0xc0|(c>>6));out+=static_cast<char>(0x80|(c&63));}
        else if(c<65536){out+=static_cast<char>(0xe0|(c>>12));out+=static_cast<char>(0x80|((c>>6)&63));out+=static_cast<char>(0x80|(c&63));}
        else {out+=static_cast<char>(0xf0|(c>>18));out+=static_cast<char>(0x80|((c>>12)&63));out+=static_cast<char>(0x80|((c>>6)&63));out+=static_cast<char>(0x80|(c&63));}
    }
    return out;
}
struct Stream final : IBStream {
    std::vector<std::byte> bytes; std::size_t offset=0,limit=64u*1024u*1024u;
    bool failed=false;
    tresult PLUGIN_API queryInterface(const TUID id,void** out) override {*out=nullptr;if(same(id,IBStream::iid)||same(id,FUnknown::iid)){*out=this;return kResultOk;}return kNoInterface;}
    uint32 PLUGIN_API addRef() override{return 1;} uint32 PLUGIN_API release() override{return 1;}
    tresult PLUGIN_API read(void* out,int32 n,int32* actual) override {
        if(actual)*actual=0;if(n<0||(!out&&n))return kInvalidArgument;
        auto count=std::min<std::size_t>(n,bytes.size()-offset);
        if(count)std::memcpy(out,bytes.data()+offset,count);offset+=count;if(actual)*actual=static_cast<int32>(count);
        return count? kResultOk:kResultFalse;
    }
    tresult PLUGIN_API write(void* data,int32 n,int32* actual) override {
        if(actual)*actual=0;
        if(n<0||(!data&&n)||static_cast<std::size_t>(n)>limit-offset){failed=true;return kInvalidArgument;}
        try {if(offset+n>bytes.size())bytes.resize(offset+n);}catch(...){failed=true;return kOutOfMemory;}
        if(n)std::memcpy(bytes.data()+offset,data,n);offset+=n;if(actual)*actual=n;return kResultOk;
    }
    tresult PLUGIN_API seek(int64 delta,int32 mode,int64* result) override {
        auto base=mode==kIBSeekSet?0:mode==kIBSeekCur?static_cast<int64>(offset):mode==kIBSeekEnd?static_cast<int64>(bytes.size()):-1;
        if(base<0 || delta < -base || delta>static_cast<int64>(limit)-base)return kInvalidArgument;
        auto pos=static_cast<std::size_t>(base+delta);if(pos>bytes.size())return kInvalidArgument;
        offset=pos;if(result)*result=static_cast<int64>(pos);return kResultOk;
    }
    tresult PLUGIN_API tell(int64* result) override {if(!result)return kInvalidArgument;*result=static_cast<int64>(offset);return kResultOk;}
};
struct EventList final : IEventList {
    std::vector<Steinberg::Vst::Event> values; std::size_t count=0; bool dropped=false;
    tresult PLUGIN_API queryInterface(const TUID,void** out) override{*out=nullptr;return kNoInterface;}
    uint32 PLUGIN_API addRef() override{return 1;}uint32 PLUGIN_API release() override{return 1;}
    int32 PLUGIN_API getEventCount() override{return static_cast<int32>(count);}
    tresult PLUGIN_API getEvent(int32 n,Steinberg::Vst::Event& e) override{if(n<0||static_cast<std::size_t>(n)>=count)return kInvalidArgument;e=values[n];return kResultOk;}
    tresult PLUGIN_API addEvent(Steinberg::Vst::Event& e) override{
        if(count>=values.size()){dropped=true;return kOutOfMemory;}values[count++]=e;return kResultOk;
    }
};
struct Changes final : IParameterChanges {
    struct Point{int32 frame;double value;int next=-1;};
    struct Queue final : IParamValueQueue {
        Changes* owner=nullptr;ParamID id=0;int first=-1,last=-1,count=0;
        tresult PLUGIN_API queryInterface(const TUID,void** out) override{*out=nullptr;return kNoInterface;}
        uint32 PLUGIN_API addRef() override{return 1;}uint32 PLUGIN_API release() override{return 1;}
        ParamID PLUGIN_API getParameterId() override{return id;}int32 PLUGIN_API getPointCount() override{return count;}
        tresult PLUGIN_API getPoint(int32 i,int32& frame,ParamValue& value) override {
            if(i<0||i>=count)return kInvalidArgument;auto n=first;while(i--)n=owner->points[n].next;
            frame=owner->points[n].frame;value=owner->points[n].value;return kResultOk;
        }
        tresult PLUGIN_API addPoint(int32 frame,ParamValue value,int32& index) override {
            if(owner->used>=owner->points.size()){owner->dropped=true;return kOutOfMemory;}
            auto n=static_cast<int>(owner->used++);owner->points[n]={frame,value,-1};
            if(last>=0)owner->points[last].next=n;else first=n;last=n;index=count++;return kResultOk;
        }
    };
    std::vector<Point> points;std::vector<Queue> queues;std::size_t used=0,count=0;bool dropped=false;
    void reset(){used=count=0;dropped=false;}
    tresult PLUGIN_API queryInterface(const TUID,void** out) override{*out=nullptr;return kNoInterface;}
    uint32 PLUGIN_API addRef() override{return 1;}uint32 PLUGIN_API release() override{return 1;}
    int32 PLUGIN_API getParameterCount() override{return static_cast<int32>(count);}
    IParamValueQueue* PLUGIN_API getParameterData(int32 n) override{return n>=0&&static_cast<std::size_t>(n)<count?&queues[n]:nullptr;}
    IParamValueQueue* PLUGIN_API addParameterData(const ParamID& id,int32& index) override {
        for(std::size_t i=0;i<count;++i)if(queues[i].id==id){index=static_cast<int32>(i);return &queues[i];}
        if(count>=queues.size()){dropped=true;return nullptr;}auto& q=queues[count];q.owner=this;q.id=id;q.first=q.last=-1;q.count=0;
        index=static_cast<int32>(count++);return &q;
    }
};
struct Vst final : HostedBackend,IHostApplication,IComponentHandler,IPlugFrame {
#ifdef _WIN32
    HMODULE module=nullptr;
#elif defined(__APPLE__)
    CFBundleRef module=nullptr;
#else
    void* module=nullptr;
#endif
    bool initializedModule=false;
    IPluginFactory* factory=nullptr;IComponent* component=nullptr;IAudioProcessor* processor=nullptr;IEditController* controller=nullptr;
    bool initializedController=false;
    IPlugView* view=nullptr;bool attached=false;WindowApi editorApi=WindowApi::win32;
#ifdef _WIN32
    HWND editorContainer=nullptr;
#endif
    static const char* platform(WindowApi a){return a==WindowApi::win32?kPlatformTypeHWND:a==WindowApi::cocoa?kPlatformTypeNSView:kPlatformTypeX11EmbedWindowID;}
    IConnectionPoint *componentConnection=nullptr,*controllerConnection=nullptr;
    std::atomic<std::uint32_t> requests{0};
    EventList inputEvents,outputEvents;Changes inputChanges,outputChanges;
    std::vector<AudioBusBuffers> inputBuses,outputBuses;
    bool(*shutdown)()=nullptr;
    tresult PLUGIN_API queryInterface(const TUID id,void** out) override {
        *out=nullptr;
        if(same(id,IHostApplication::iid)||same(id,FUnknown::iid))*out=static_cast<IHostApplication*>(this);
        else if(same(id,IComponentHandler::iid))*out=static_cast<IComponentHandler*>(this);
        else if(same(id,IPlugFrame::iid))*out=static_cast<IPlugFrame*>(this);
        return *out?kResultOk:kNoInterface;
    }
    uint32 PLUGIN_API addRef() override{return 1;}uint32 PLUGIN_API release() override{return 1;}
    tresult PLUGIN_API getName(String128 name) override {const char16_t text[]=u"CyCLAPBridge";std::copy(std::begin(text),std::end(text),name);return kResultOk;}
    tresult PLUGIN_API createInstance(TUID,TUID,void** out) override{*out=nullptr;return kNoInterface;}
    tresult PLUGIN_API beginEdit(ParamID id) override{edited(id,controller?controller->getParamNormalized(id):0,EditGesture::begin);return kResultOk;}
    tresult PLUGIN_API endEdit(ParamID id) override{edited(id,controller?controller->getParamNormalized(id):0,EditGesture::end);return kResultOk;}
    tresult PLUGIN_API performEdit(ParamID id,ParamValue value) override {edited(id,value,EditGesture::value);requests.fetch_or(parametersChanged);return kResultOk;}
    tresult PLUGIN_API resizeView(IPlugView* incoming,ViewRect* rect) override {
        if(owner!=std::this_thread::get_id()||incoming!=view||!rect||rect->right<=rect->left||rect->bottom<=rect->top)return kInvalidArgument;
        EditorSize size{static_cast<std::uint32_t>(rect->right-rect->left),static_cast<std::uint32_t>(rect->bottom-rect->top)};
        if(!validSize(size)||!editorCallbacks.requestResize||!editorCallbacks.requestResize(editorCallbacks.context,size))return kResultFalse;
#ifdef _WIN32
        if(editorContainer)MoveWindow(editorContainer,0,0,size.width,size.height,TRUE);
#endif
        return view->onSize(rect);
    }
    bool editorSupported(WindowApi a) const override {
#ifdef _WIN32
        if(a!=WindowApi::win32)return false;
#else
        (void)a;return false; // Native UI validation on other platforms is deferred.
#endif
        if(view)return view->isPlatformTypeSupported(platform(a))==kResultTrue;
        if(!controller)return false;auto* candidate=controller->createView(ViewType::kEditor);if(!candidate)return false;
        bool supported=candidate->isPlatformTypeSupported(platform(a))==kResultTrue;candidate->release();return supported;
    }
    bool createEditor(WindowApi a,const EditorCallbacks& c) override {
        if(view||!controller||!editorSupported(a))return false;view=controller->createView(ViewType::kEditor);
        if(!view)return false;
        if(view->isPlatformTypeSupported(platform(a))!=kResultTrue){view->release();view=nullptr;return false;}
        editorCallbacks=c;editorApi=a;view->setFrame(this);return true;
    }
    void destroyEditor() override {
        if(view){if(attached)view->removed();attached=false;view->setFrame(nullptr);view->release();view=nullptr;}
#ifdef _WIN32
        if(editorContainer){DestroyWindow(editorContainer);editorContainer=nullptr;}
#endif
        editorCallbacks={};
    }
    EditorSize editorSize() const override {
        ViewRect r{};if(!view||view->getSize(&r)!=kResultOk||r.right<=r.left||r.bottom<=r.top)return {};
        return {static_cast<std::uint32_t>(r.right-r.left),static_cast<std::uint32_t>(r.bottom-r.top)};
    }
    bool editorCanResize() const override{return view&&view->canResize()==kResultTrue;}
    bool adjustEditorSize(EditorSize& s) const override{
        if(!view)return false;ViewRect r{0,0,static_cast<int32>(s.width),static_cast<int32>(s.height)};
        if(view->checkSizeConstraint(&r)!=kResultTrue||r.right<=r.left||r.bottom<=r.top)return false;
        s={static_cast<std::uint32_t>(r.right-r.left),static_cast<std::uint32_t>(r.bottom-r.top)};return validSize(s);
    }
    bool resizeEditor(EditorSize s) override{
        if(!view||!editorCanResize())return false;ViewRect r{0,0,static_cast<int32>(s.width),static_cast<int32>(s.height)};
        if(view->onSize(&r)!=kResultOk)return false;
#ifdef _WIN32
        if(editorContainer)MoveWindow(editorContainer,0,0,s.width,s.height,TRUE);
#endif
        return true;
    }
    bool setEditorParent(NativeWindow p) override{
        if(!view||attached||p.api!=editorApi)return false;
#ifdef _WIN32
        auto size=editorSize();
        editorContainer=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,0,0,size.width,size.height,static_cast<HWND>(p.handle),nullptr,GetModuleHandleW(nullptr),nullptr);
        if(!editorContainer)return false;
        attached=view->attached(editorContainer,platform(p.api))==kResultOk;
        if(!attached){DestroyWindow(editorContainer);editorContainer=nullptr;}
#endif
        return attached;
    }
    bool setEditorScale(double s) override{
        if(!view)return false;IPlugViewContentScaleSupport* scale=nullptr;
        if(view->queryInterface(IPlugViewContentScaleSupport::iid,reinterpret_cast<void**>(&scale))!=kResultOk)return false;
        auto result=scale->setContentScaleFactor(static_cast<float>(s));scale->release();return result==kResultTrue;
    }
    bool showEditor(bool visible) override {
        if(!attached)return false;
#ifdef _WIN32
        ShowWindow(editorContainer,visible?SW_SHOWNA:SW_HIDE);
#else
        (void)visible;
#endif
        return true;
    }
    void editorParameter(std::uint32_t id,double v) override{if(controller)controller->setParamNormalized(id,v);}
    double currentParameter(std::uint32_t id) const override{return controller?controller->getParamNormalized(id):0;}
    tresult PLUGIN_API restartComponent(int32) override{requests.fetch_or(restartRequested);return kResultOk;}
    std::uint32_t pump() override{control();return requests.exchange(0);}
    explicit Vst(const std::filesystem::path& source,const std::string& id) {
        try {
            auto path=source;
#if !defined(__APPLE__)
            if(std::filesystem::is_directory(path)) {
#ifdef _WIN32
                auto arch=sizeof(void*)==8?"x86_64-win":"x86-win";
#elif defined(__APPLE__)
                auto arch="MacOS";
#else
                auto arch="x86_64-linux";
#endif
                auto folder=path/"Contents"/arch;
                bool found=false;
                for(const auto& entry:std::filesystem::directory_iterator(folder))if(entry.is_regular_file()){path=entry.path();found=true;break;}
                if(!found)throw std::runtime_error("VST3 bundle has no native module");
            }
#endif
#ifdef _WIN32
            module=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            auto symbol=[&](const char* name){return reinterpret_cast<void*>(GetProcAddress(module,name));};
#elif defined(__APPLE__)
            auto bytes=utf8(path);
            auto url=CFURLCreateFromFileSystemRepresentation(nullptr,reinterpret_cast<const UInt8*>(bytes.data()),bytes.size(),true);
            if(url){module=CFBundleCreate(nullptr,url);CFRelease(url);}
            if(module&&!CFBundleLoadExecutable(module)){CFRelease(module);module=nullptr;}
            auto symbol=[&](const char* name){
                auto text=CFStringCreateWithCString(nullptr,name,kCFStringEncodingUTF8);
                auto result=CFBundleGetFunctionPointerForName(module,text);CFRelease(text);
                return reinterpret_cast<void*>(result);
            };
#else
            module=dlopen(path.c_str(),RTLD_LOCAL|RTLD_NOW);
            auto symbol=[&](const char* name){return dlsym(module,name);};
#endif
            if(!module)throw std::runtime_error("VST3 native module load failed");
#ifdef _WIN32
            auto initialize=reinterpret_cast<bool(*)()>(symbol("InitDll"));
            shutdown=reinterpret_cast<bool(*)()>(symbol("ExitDll"));
            initializedModule=initialize&&initialize();
#else
            auto initialize=reinterpret_cast<bool(*)(void*)>(symbol(
#ifdef __APPLE__
                "bundleEntry"
#else
                "ModuleEntry"
#endif
            ));
            shutdown=reinterpret_cast<bool(*)()>(symbol(
#ifdef __APPLE__
                "bundleExit"
#else
                "ModuleExit"
#endif
            ));
            initializedModule=initialize&&initialize(module);
#endif
            auto entry=reinterpret_cast<IPluginFactory*(*)()>(symbol("GetPluginFactory"));
            if(!initializedModule||!entry)throw std::runtime_error("Invalid VST3 entrypoints");
            factory=entry();if(!factory)throw std::runtime_error("VST3 factory failed");
            PClassInfo c{};bool found=false;char uid[33]{};
            for(int32 i=0;i<factory->countClasses();++i){
                if(factory->getClassInfo(i,&c)!=kResultOk||std::strcmp(c.category,kVstAudioEffectClass))continue;
                static const char hex[]="0123456789abcdef";
                for(int n=0;n<16;++n){auto v=static_cast<unsigned char>(c.cid[n]);uid[n*2]=hex[v>>4];uid[n*2+1]=hex[v&15];}
                if(id.empty()||id==uid){found=true;break;}
            }
            if(!found)throw std::runtime_error("VST3 class ID not found");
            PFactoryInfo f{};factory->getFactoryInfo(&f);info={uid,c.name,f.vendor,"1"};
            if(factory->createInstance(c.cid,IComponent::iid,reinterpret_cast<void**>(&component))!=kResultOk ||
                component->initialize(static_cast<IHostApplication*>(this))!=kResultOk)throw std::runtime_error("VST3 component initialization failed");
            if(component->queryInterface(IAudioProcessor::iid,reinterpret_cast<void**>(&processor))!=kResultOk)throw std::runtime_error("VST3 audio processor missing");
            if(component->queryInterface(IEditController::iid,reinterpret_cast<void**>(&controller))!=kResultOk) {
                TUID cid{};
                if(component->getControllerClassId(cid)==kResultOk &&
                    factory->createInstance(cid,IEditController::iid,reinterpret_cast<void**>(&controller))==kResultOk) {
                    if(controller->initialize(static_cast<IHostApplication*>(this))!=kResultOk)throw std::runtime_error("VST3 controller initialization failed");
                    initializedController=true;
                }
            }
            if(controller) {
                controller->setComponentHandler(this);
                component->queryInterface(IConnectionPoint::iid,reinterpret_cast<void**>(&componentConnection));
                controller->queryInterface(IConnectionPoint::iid,reinterpret_cast<void**>(&controllerConnection));
                if(componentConnection&&controllerConnection){componentConnection->connect(controllerConnection);controllerConnection->connect(componentConnection);}
                Stream state;if(component->getState(&state)==kResultOk){state.offset=0;controller->setComponentState(&state);}
                auto count=controller->getParameterCount();if(count<0||count>65536)throw std::runtime_error("VST3 parameter limit");
                for(int32 i=0;i<count;++i){ParameterInfo p{};if(controller->getParameterInfo(i,p)==kResultOk)params.push_back({p.id,utf16(p.title),0,1,p.defaultNormalizedValue,bool(p.flags&ParameterInfo::kCanAutomate),p.stepCount>0});}
            }
            for(auto direction:{kInput,kOutput}) {
                auto count=component->getBusCount(kAudio,direction);if(count<0||count>128)throw std::runtime_error("VST3 bus limit");
                auto& ports=direction==kInput?ins:outs;
                for(int32 i=0;i<count;++i){BusInfo b{};if(component->getBusInfo(kAudio,direction,i,b)!=kResultOk||b.channelCount<1||b.channelCount>64)throw std::runtime_error("Unsupported VST3 audio bus");ports.push_back({static_cast<std::uint32_t>(i),static_cast<std::uint32_t>(b.channelCount),utf16(b.name)});}
            }
        }catch(...){cleanup();throw;}
    }
    void cleanup() {
        destroyEditor();
        if(processor){if(running)processor->setProcessing(false);if(prepared)component->setActive(false);}
        if(componentConnection&&controllerConnection){componentConnection->disconnect(controllerConnection);controllerConnection->disconnect(componentConnection);}
        if(componentConnection)componentConnection->release();if(controllerConnection)controllerConnection->release();
        if(controller){controller->setComponentHandler(nullptr);if(initializedController)controller->terminate();controller->release();}
        if(processor)processor->release();if(component){component->terminate();component->release();}if(factory)factory->release();
        if(initializedModule&&shutdown)shutdown();
#ifdef _WIN32
        if(module)FreeLibrary(module);
#elif defined(__APPLE__)
        if(module){CFBundleUnloadExecutable(module);CFRelease(module);}
#else
        if(module)dlclose(module);
#endif
    }
    ~Vst() override{cleanup();}
    void prepare(double rate,std::uint32_t f,std::uint32_t e) override {
        limits(rate,f,e);if(processor->canProcessSampleSize(kSample32)!=kResultTrue)throw std::runtime_error("VST3 float32 not supported");
        inputEvents.values.resize(e);outputEvents.values.resize(e);
        inputChanges.points.resize(e);inputChanges.queues.resize(e);outputChanges.points.resize(e);outputChanges.queues.resize(e);
        inputBuses.resize(ins.size());outputBuses.resize(outs.size());
        ProcessSetup setup{kRealtime,kSample32,static_cast<int32>(f),rate};
        if(processor->setupProcessing(setup)!=kResultOk)throw std::runtime_error("VST3 processing setup failed");
        for(auto direction:{kInput,kOutput}) {
            auto n=component->getBusCount(kAudio,direction);for(int32 i=0;i<n;++i)component->activateBus(kAudio,direction,i,true);
            n=component->getBusCount(kEvent,direction);for(int32 i=0;i<n;++i)component->activateBus(kEvent,direction,i,true);
        }
        if(component->setActive(true)!=kResultOk)throw std::runtime_error("VST3 activation failed");
        latency=processor->getLatencySamples();prepared=true;
    }
    void unprepare() override{idle();if(prepared)component->setActive(false);prepared=false;}
    bool start() noexcept override{if(!prepared||running)return false;running=processor->setProcessing(true)==kResultOk;return running;}
    void stop() noexcept override{if(running)processor->setProcessing(false);running=false;}
    ProcessStatus process(ProcessBlock& b) noexcept override {
        if(!valid(b))return running?ProcessStatus::invalidBlock:ProcessStatus::notPrepared;
        inputChanges.reset();outputChanges.reset();inputEvents.count=outputEvents.count=0;inputEvents.dropped=outputEvents.dropped=false;
        for(const auto& e:b.events) {
            if(e.kind==EventKind::parameter){int32 index=0;auto* q=inputChanges.addParameterData(e.parameterId,index);if(!q||q->addPoint(e.frame,e.value,index)!=kResultOk)return ProcessStatus::invalidBlock;}
            else {
                Steinberg::Vst::Event n{};n.busIndex=e.port;n.sampleOffset=e.frame;
                if(e.kind==EventKind::noteOn){n.type=Steinberg::Vst::Event::kNoteOnEvent;n.noteOn={e.channel,e.key,0,static_cast<float>(e.value),0,e.noteId};}
                else if(e.kind==EventKind::noteOff){n.type=Steinberg::Vst::Event::kNoteOffEvent;n.noteOff={e.channel,e.key,static_cast<float>(e.value),e.noteId,0};}
                else if((e.midi[0]&0xf0)==0x90 || (e.midi[0]&0xf0)==0x80){
                    bool on=(e.midi[0]&0xf0)==0x90&&e.midi[2];n.type=on?Steinberg::Vst::Event::kNoteOnEvent:Steinberg::Vst::Event::kNoteOffEvent;
                    if(on)n.noteOn={static_cast<int16>(e.midi[0]&15),e.midi[1],0,e.midi[2]/127.f,0,-1};else n.noteOff={static_cast<int16>(e.midi[0]&15),e.midi[1],e.midi[2]/127.f,-1,0};
                }else return ProcessStatus::invalidBlock;
                if(inputEvents.addEvent(n)!=kResultOk)return ProcessStatus::invalidBlock;
            }
        }
        auto set=[](auto buses,auto& native){for(std::size_t i=0;i<buses.size();++i){native[i]={};native[i].numChannels=buses[i].channelCount;native[i].channelBuffers32=const_cast<float**>(buses[i].channels);}};
        set(b.inputs,inputBuses);set(b.outputs,outputBuses);
        ProcessData data{};data.processMode=kRealtime;data.symbolicSampleSize=kSample32;data.numSamples=b.frames;
        data.numInputs=static_cast<int32>(inputBuses.size());data.numOutputs=static_cast<int32>(outputBuses.size());
        data.inputs=inputBuses.data();data.outputs=outputBuses.data();data.inputEvents=&inputEvents;data.outputEvents=&outputEvents;
        data.inputParameterChanges=&inputChanges;data.outputParameterChanges=&outputChanges;
        if(processor->process(data)!=kResultOk)return ProcessStatus::pluginError;
        b.outputEventsDropped=outputEvents.dropped||outputChanges.dropped;
        for(std::size_t i=0;i<outputEvents.count;++i){const auto& n=outputEvents.values[i];cybridge::Event e;e.frame=n.sampleOffset;e.port=static_cast<std::int16_t>(n.busIndex);
            if(n.sampleOffset<0||e.frame>=b.frames){b.outputEventsDropped=true;continue;}
            if(n.type==Steinberg::Vst::Event::kNoteOnEvent){e.kind=EventKind::noteOn;e.channel=n.noteOn.channel;e.key=n.noteOn.pitch;e.value=n.noteOn.velocity;e.noteId=n.noteOn.noteId;}
            else if(n.type==Steinberg::Vst::Event::kNoteOffEvent){e.kind=EventKind::noteOff;e.channel=n.noteOff.channel;e.key=n.noteOff.pitch;e.value=n.noteOff.velocity;e.noteId=n.noteOff.noteId;}
            else{b.outputEventsDropped=true;continue;}emit(b,e);
        }
        for(std::size_t i=0;i<outputChanges.count;++i){auto& q=outputChanges.queues[i];for(int j=0;j<q.count;++j){int32 frame;double value;q.getPoint(j,frame,value);if(frame<0||static_cast<std::uint32_t>(frame)>=b.frames){b.outputEventsDropped=true;continue;}cybridge::Event e;e.frame=frame;e.parameterId=q.id;e.value=value;emit(b,e);}}
        std::sort(b.outputEvents.begin(),b.outputEvents.begin()+b.outputEventCount,[](const auto& a,const auto& c){return a.frame<c.frame;});
        return ProcessStatus::ok;
    }
    std::vector<std::byte> save(std::size_t limit) override {
        idle();Stream componentState,controllerState;componentState.limit=controllerState.limit=limit;
        if(component->getState(&componentState)!=kResultOk||componentState.failed)throw std::runtime_error("VST3 component state failed");
        if(controller&&controller->getState(&controllerState)!=kResultOk)controllerState.bytes.clear();
        std::uint64_t a=componentState.bytes.size(),b=controllerState.bytes.size();
        if(controllerState.failed||a+b+16>limit)throw std::runtime_error("VST3 state capacity exceeded");
        std::vector<std::byte> result(static_cast<std::size_t>(a+b+16));std::memcpy(result.data(),&a,8);std::memcpy(result.data()+8,&b,8);
        if(a)std::memcpy(result.data()+16,componentState.bytes.data(),a);if(b)std::memcpy(result.data()+16+a,controllerState.bytes.data(),b);return result;
    }
    void load(std::span<const std::byte> b) override {
        idle();if(b.size()<16)throw std::runtime_error("Invalid VST3 state");
        std::uint64_t n,m;std::memcpy(&n,b.data(),8);std::memcpy(&m,b.data()+8,8);
        if(n>b.size()-16||m!=b.size()-16-n)throw std::runtime_error("Invalid VST3 state bounds");
        Stream c,d;c.bytes.assign(b.begin()+16,b.begin()+16+n);d.bytes.assign(b.begin()+16+n,b.end());
        if(component->setState(&c)!=kResultOk)throw std::runtime_error("VST3 component state load failed");
        if(controller){c.offset=0;controller->setComponentState(&c);if(m&&controller->setState(&d)!=kResultOk)throw std::runtime_error("VST3 controller state load failed");}
    }
};
}
std::unique_ptr<HostedBackend> makeVst3(const std::filesystem::path& p,const std::string& id){return std::make_unique<Vst>(p,id);}
}
