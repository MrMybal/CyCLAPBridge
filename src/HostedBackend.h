// SPDX-License-Identifier: MIT
#pragma once
#include <cyclapbridge/Plugin.h>
#include <cmath>
#include <stdexcept>
#include <thread>
#include <atomic>
namespace cybridge::detail {
struct HostedBackend {
    PluginDescriptor info;
    std::vector<AudioPort> ins, outs;
    std::vector<Parameter> params;
    std::thread::id owner = std::this_thread::get_id();
    bool prepared=false;std::atomic<bool> running{false};
    std::uint32_t maximumFrames=0, maximumEvents=0, latency=0;
    EditorCallbacks editorCallbacks{};
    struct ParameterMirror { std::atomic<double> value{0},pendingValue{0}; std::atomic<bool> pending{false},changed{false}; };
    std::unique_ptr<ParameterMirror[]> mirror;
    void initializeMirrors() {
        mirror=std::make_unique<ParameterMirror[]>(params.size());
        for(std::size_t i=0;i<params.size();++i)mirror[i].value.store(currentParameter(params[i].id));
    }
    void edited(std::uint32_t id,double value,EditGesture gesture) noexcept {
        if(owner!=std::this_thread::get_id() || !std::isfinite(value))return;
        for(std::size_t i=0;i<params.size();++i)if(params[i].id==id){
            if(value<params[i].minimum||value>params[i].maximum)return;
            if(gesture==EditGesture::value){mirror[i].value.store(value);mirror[i].pendingValue.store(value);mirror[i].pending.store(true,std::memory_order_release);}
            if(editorCallbacks.parameterEdit)editorCallbacks.parameterEdit(editorCallbacks.context,id,value,gesture);
            return;
        }
    }
    void record(std::span<const Event> events) noexcept {
        for(auto& e:events)if(e.kind==EventKind::parameter)for(std::size_t i=0;i<params.size();++i)if(params[i].id==e.parameterId){
            mirror[i].value.store(e.value,std::memory_order_relaxed);mirror[i].changed.store(true,std::memory_order_release);break;
        }
    }
    void synchronizeEditor() {
        control();for(std::size_t i=0;i<params.size();++i)if(mirror[i].changed.exchange(false,std::memory_order_acquire))
            editorParameter(params[i].id,mirror[i].value.load(std::memory_order_relaxed));
    }
    static bool validSize(EditorSize s) {return s.width>0&&s.height>0&&s.width<=8192&&s.height<=8192;}
    virtual ~HostedBackend() = default;
    static_assert(std::atomic<double>::is_always_lock_free);
    void control() const { if(owner!=std::this_thread::get_id()) throw std::logic_error("Wrong plugin control thread"); }
    void idle() const { control(); if(running) throw std::logic_error("Stop processing first"); }
    void limits(double rate,std::uint32_t frames,std::uint32_t events) {
        idle();
        if(prepared || !std::isfinite(rate) || rate<=0 || !frames || frames>1048576 || events>1048576)
            throw std::invalid_argument("Invalid preparation or already prepared");
        maximumFrames=frames; maximumEvents=events;
    }
    bool valid(ProcessBlock& b) const noexcept {
        b.outputEventCount=0; b.outputEventsDropped=false;
        if(!running || !b.frames || b.frames>maximumFrames || b.events.size()>maximumEvents ||
            b.inputs.size()!=ins.size() || b.outputs.size()!=outs.size()) return false;
        auto buses=[](auto incoming,const auto& ports) {
            for(std::size_t i=0;i<incoming.size();++i) {
                if(incoming[i].channelCount!=ports[i].channels || !incoming[i].channels) return false;
                for(std::uint32_t c=0;c<incoming[i].channelCount;++c) if(!incoming[i].channels[c]) return false;
            }
            return true;
        };
        if(!buses(b.inputs,ins) || !buses(b.outputs,outs)) return false;
        std::uint32_t last=0;
        for(const auto& e:b.events) {
            if(e.frame>=b.frames || e.frame<last || !std::isfinite(e.value)) return false;
            last=e.frame;
            if(e.kind==EventKind::parameter) {
                bool found=false;
                for(const auto& p:params) if(p.id==e.parameterId) {
                    found=e.value>=p.minimum && e.value<=p.maximum; break;
                }
                if(!found) return false;
            } else if(e.kind==EventKind::noteOn || e.kind==EventKind::noteOff) {
                if(e.key<0 || e.key>127 || e.channel<0 || e.channel>15 || e.value<0 || e.value>1) return false;
            } else if(e.kind!=EventKind::midi) return false;
        }
        return true;
    }
    static void emit(ProcessBlock& b,const Event& e) noexcept {
        if(b.outputEventCount<b.outputEvents.size()) b.outputEvents[b.outputEventCount++]=e;
        else b.outputEventsDropped=true;
    }
    virtual void prepare(double,std::uint32_t,std::uint32_t)=0;
    virtual void unprepare()=0;
    virtual bool start() noexcept { if(!prepared || running) return false; return running=true; }
    virtual void stop() noexcept { running=false; }
    virtual ProcessStatus process(ProcessBlock&) noexcept=0;
    virtual std::uint32_t pump() { control(); return 0; }
    virtual std::vector<std::byte> save(std::size_t)=0;
    virtual void load(std::span<const std::byte>)=0;
    virtual double currentParameter(std::uint32_t id) const {for(auto& p:params)if(p.id==id)return p.defaultValue;return 0;}
    virtual double editorValue(std::uint32_t id) const {return currentParameter(id);}
    virtual bool editorSupported(WindowApi) const {return false;}
    virtual bool createEditor(WindowApi,const EditorCallbacks&){return false;}
    virtual void destroyEditor() {}
    virtual EditorSize editorSize() const {return {};}
    virtual bool editorCanResize() const {return false;}
    virtual bool adjustEditorSize(EditorSize&) const {return false;}
    virtual bool resizeEditor(EditorSize){return false;}
    virtual bool setEditorParent(NativeWindow){return false;}
    virtual bool setEditorScale(double){return false;}
    virtual bool showEditor(bool){return false;}
    virtual void editorParameter(std::uint32_t,double){}
};
std::unique_ptr<HostedBackend> makeVst3(const std::filesystem::path&,const std::string&);
std::unique_ptr<HostedBackend> makeLv2(const std::filesystem::path&,const std::string&);
std::unique_ptr<HostedBackend> makeJsfx(const std::filesystem::path&);
std::unique_ptr<HostedBackend> makeAu(const std::filesystem::path&,const std::string&);
}
