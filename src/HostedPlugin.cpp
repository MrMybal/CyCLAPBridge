// SPDX-License-Identifier: MIT
#include "HostedBackend.h"
#include <cstring>
namespace cybridge {
namespace {
struct ClapBackend final : detail::HostedBackend {
    ClapPlugin plugin;
    ClapBackend(const std::filesystem::path& p,const std::string& id):plugin(p,id) {
        info=plugin.descriptor(); ins=plugin.inputs(); outs=plugin.outputs(); params=plugin.parameters();
    }
    void prepare(double rate,std::uint32_t f,std::uint32_t e) override {
        limits(rate,f,e); plugin.prepare(rate,f,e); latency=plugin.latencyFrames(); prepared=true;
    }
    void unprepare() override { idle(); plugin.release(); prepared=false; }
    bool start() noexcept override { running=plugin.start(); return running; }
    void stop() noexcept override { plugin.stop(); running=false; }
    ProcessStatus process(ProcessBlock& b) noexcept override { return plugin.process(b); }
    std::uint32_t pump() override { return plugin.pumpMainThread(); }
    std::vector<std::byte> save(std::size_t n) override { return plugin.saveState(n); }
    void load(std::span<const std::byte> b) override { plugin.loadState(b); }
    double currentParameter(std::uint32_t id) const override{double value=0;plugin.parameterValue(id,value);return value;}
    bool editorSupported(WindowApi a) const override{return plugin.editorSupported(a);}
    bool createEditor(WindowApi a,const EditorCallbacks& c) override{
        auto previous=editorCallbacks;editorCallbacks=c;
        auto result=plugin.createEditor(a,{this,
            [](void* context,EditorSize size) noexcept {auto& s=*static_cast<ClapBackend*>(context);return s.editorCallbacks.requestResize&&s.editorCallbacks.requestResize(s.editorCallbacks.context,size);},
            [](void* context,std::uint32_t id,double value,EditGesture gesture) noexcept{static_cast<ClapBackend*>(context)->edited(id,value,gesture);},
            [](void* context) noexcept{auto& s=*static_cast<ClapBackend*>(context);if(s.editorCallbacks.closed)s.editorCallbacks.closed(s.editorCallbacks.context);}});
        if(!result)editorCallbacks=previous;return result;
    }
    void destroyEditor() override{plugin.destroyEditor();}
    EditorSize editorSize() const override{return plugin.editorSize();}
    bool editorCanResize() const override{return plugin.editorCanResize();}
    bool adjustEditorSize(EditorSize& s) const override{return plugin.adjustEditorSize(s);}
    bool resizeEditor(EditorSize s) override{return plugin.resizeEditor(s);}
    bool setEditorParent(NativeWindow p) override{return plugin.setEditorParent(p);}
    bool setEditorScale(double s) override{return plugin.setEditorScale(s);}
    bool showEditor(bool v) override{return plugin.showEditor(v);}
};
}
struct HostedPlugin::Impl {
    std::unique_ptr<detail::HostedBackend> backend;
    std::vector<Event> merged;
    std::uint32_t eventLimit=0;
};
HostedPlugin::HostedPlugin(Format f,const std::filesystem::path& p,const std::string& id):impl_(std::make_unique<Impl>()) {
    if(!p.is_absolute()) throw std::invalid_argument("Plugin path must be absolute");
    switch(f) {
    case Format::clap: impl_->backend=std::make_unique<ClapBackend>(p,id); break;
    case Format::vst3: impl_->backend=detail::makeVst3(p,id); break;
    case Format::lv2: impl_->backend=detail::makeLv2(p,id); break;
    case Format::jsfx: impl_->backend=detail::makeJsfx(p); break;
    case Format::auv2: case Format::auv3:
#ifdef __APPLE__
        impl_->backend=detail::makeAu(p,id); break;
#else
        throw std::runtime_error("Audio Unit hosting requires macOS");
#endif
    }
    impl_->backend->initializeMirrors();
}
HostedPlugin::~HostedPlugin()=default;
const PluginDescriptor& HostedPlugin::descriptor() const noexcept { return impl_->backend->info; }
const std::vector<AudioPort>& HostedPlugin::inputs() const noexcept { return impl_->backend->ins; }
const std::vector<AudioPort>& HostedPlugin::outputs() const noexcept { return impl_->backend->outs; }
const std::vector<Parameter>& HostedPlugin::parameters() const noexcept { return impl_->backend->params; }
bool HostedPlugin::parameterValue(std::uint32_t id,double& value) const{auto& b=*impl_->backend;b.control();for(std::size_t i=0;i<b.params.size();++i)if(b.params[i].id==id){value=b.mirror[i].pending.load()?b.mirror[i].pendingValue.load():b.editorValue(id);return true;}return false;}
void HostedPlugin::prepare(double r,std::uint32_t f,std::uint32_t e) {
    if(e>1048576)throw std::invalid_argument("Event capacity too large");
    impl_->backend->idle();if(impl_->backend->prepared)throw std::logic_error("Already prepared");impl_->eventLimit=e;
    impl_->merged.resize(static_cast<std::size_t>(e)+impl_->backend->params.size());
    impl_->backend->prepare(r,f,static_cast<std::uint32_t>(impl_->merged.size()));
}
void HostedPlugin::release() { impl_->backend->unprepare(); }
bool HostedPlugin::start() noexcept { return impl_->backend->start(); }
void HostedPlugin::stop() noexcept { impl_->backend->stop(); }
ProcessStatus HostedPlugin::process(ProcessBlock& b) noexcept {
    auto& s=*impl_;auto& backend=*s.backend;
    // Validate before consuming GUI edits or updating any mirrored value.
    if(b.events.size()>s.eventLimit||!backend.valid(b))return backend.running?ProcessStatus::invalidBlock:ProcessStatus::notPrepared;
    std::size_t count=0;
    for(std::size_t i=0;i<backend.params.size();++i)if(backend.mirror[i].pending.exchange(false,std::memory_order_acquire)){
        auto& e=s.merged[count++];e={};e.parameterId=backend.params[i].id;e.value=backend.mirror[i].pendingValue.load();
    }
    for(auto& e:b.events)s.merged[count++]=e;
    auto original=b.events;b.events={s.merged.data(),count};
    auto result=backend.process(b);
    if(result==ProcessStatus::ok){backend.record(b.events);backend.record(b.outputEvents.first(b.outputEventCount));}
    b.events=original;return result;
}
std::uint32_t HostedPlugin::latencyFrames() const noexcept { return impl_->backend->latency; }
std::uint32_t HostedPlugin::pumpMainThread() { impl_->backend->synchronizeEditor();return impl_->backend->pump(); }
std::vector<std::byte> HostedPlugin::saveState(std::size_t n) {
    if(n>64u*1024u*1024u) throw std::invalid_argument("State limit exceeds 64 MiB");
    auto& backend=*impl_->backend;backend.idle();
    auto count=static_cast<std::uint32_t>(backend.params.size());auto header=12+static_cast<std::size_t>(count)*16;
    if(header>n)throw std::runtime_error("State capacity exceeded");
    auto native=backend.save(n-header);std::vector<std::byte> result(header+native.size());
    std::memcpy(result.data(),"CYBHST02",8);std::memcpy(result.data()+8,&count,4);
    for(std::size_t i=0;i<count;++i){auto id=backend.params[i].id;std::uint32_t pending=backend.mirror[i].pending.load()?1:0;auto value=pending?backend.mirror[i].pendingValue.load():backend.mirror[i].value.load();std::memcpy(result.data()+12+i*16,&id,4);std::memcpy(result.data()+16+i*16,&pending,4);std::memcpy(result.data()+20+i*16,&value,8);}
    if(!native.empty())std::memcpy(result.data()+header,native.data(),native.size());return result;
}
void HostedPlugin::loadState(std::span<const std::byte> b) {
    if(b.size()>64u*1024u*1024u) throw std::invalid_argument("State exceeds 64 MiB");
    auto& backend=*impl_->backend;backend.idle();
    bool v2=b.size()>=8&&!std::memcmp(b.data(),"CYBHST02",8);
    if(!v2&&(b.size()<8||std::memcmp(b.data(),"CYBHST01",8))){backend.load(b);for(std::size_t i=0;i<backend.params.size();++i){backend.mirror[i].pending.store(false);backend.mirror[i].value.store(backend.currentParameter(backend.params[i].id));backend.mirror[i].changed.store(true);}return;}
    if(b.size()<12)throw std::runtime_error("Truncated host state");
    auto stride=v2?16u:12u;std::uint32_t count;std::memcpy(&count,b.data()+8,4);auto header=12+static_cast<std::size_t>(count)*stride;
    if(count!=backend.params.size()||header>b.size())throw std::runtime_error("Host state parameter bounds");
    for(std::size_t i=0;i<count;++i){std::uint32_t id,pending=1;double value;std::memcpy(&id,b.data()+12+i*stride,4);if(v2)std::memcpy(&pending,b.data()+16+i*stride,4);std::memcpy(&value,b.data()+(v2?20:16)+i*stride,8);
        if(pending>1||id!=backend.params[i].id||!std::isfinite(value)||value<backend.params[i].minimum||value>backend.params[i].maximum)throw std::runtime_error("Invalid host parameter state");}
    backend.load(b.subspan(header));
    for(std::size_t i=0;i<count;++i){double value;std::uint32_t pending=1;if(v2)std::memcpy(&pending,b.data()+16+i*stride,4);std::memcpy(&value,b.data()+(v2?20:16)+i*stride,8);if(!pending)value=backend.currentParameter(backend.params[i].id);backend.mirror[i].value.store(value);backend.mirror[i].pendingValue.store(value);backend.mirror[i].pending.store(pending!=0);backend.mirror[i].changed.store(true);}
}
bool HostedPlugin::editorSupported(WindowApi a) const {impl_->backend->control();return impl_->backend->editorSupported(a);}
bool HostedPlugin::createEditor(WindowApi a,const EditorCallbacks& c){impl_->backend->control();return impl_->backend->createEditor(a,c);}
void HostedPlugin::destroyEditor(){impl_->backend->control();impl_->backend->destroyEditor();}
EditorSize HostedPlugin::editorSize() const{impl_->backend->control();return impl_->backend->editorSize();}
bool HostedPlugin::editorCanResize() const{impl_->backend->control();return impl_->backend->editorCanResize();}
bool HostedPlugin::adjustEditorSize(EditorSize& s) const{impl_->backend->control();return detail::HostedBackend::validSize(s)&&impl_->backend->adjustEditorSize(s);}
bool HostedPlugin::resizeEditor(EditorSize s){impl_->backend->control();return detail::HostedBackend::validSize(s)&&impl_->backend->resizeEditor(s);}
bool HostedPlugin::setEditorParent(NativeWindow p){impl_->backend->control();return p.handle&&impl_->backend->setEditorParent(p);}
bool HostedPlugin::setEditorScale(double s){impl_->backend->control();return std::isfinite(s)&&s>0&&s<=8&&impl_->backend->setEditorScale(s);}
bool HostedPlugin::showEditor(bool v){impl_->backend->control();return impl_->backend->showEditor(v);}
}
