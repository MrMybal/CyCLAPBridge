// SPDX-License-Identifier: MIT
#include <cyclapbridge/Plugin.h>
#include "Process.h"
#include <clap/clap.h>
#include <algorithm>
#include <atomic>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#else
#include <dlfcn.h>
#endif
namespace cybridge {
namespace {
std::string safe(const char* s) { return s ? s : ""; }
struct Module {
#ifdef _WIN32
    HMODULE handle = nullptr;
#elif defined(__APPLE__)
    CFBundleRef handle = nullptr;
#else
    void* handle = nullptr;
#endif
    const clap_plugin_entry_t* entry = nullptr;
    bool initialized = false;
    ~Module() {
        if (initialized) entry->deinit();
#ifdef _WIN32
        if(handle) FreeLibrary(handle);
#elif defined(__APPLE__)
        if(handle) { CFBundleUnloadExecutable(handle); CFRelease(handle); }
#else
        if(handle) dlclose(handle);
#endif
    }
};
std::shared_ptr<Module> loadModule(const std::filesystem::path& input) {
    if (!input.is_absolute()) throw std::invalid_argument("CLAP module path must be absolute");
    auto path = std::filesystem::canonical(input);
    static std::mutex mutex;
    static std::map<std::filesystem::path, std::weak_ptr<Module>> modules;
    std::lock_guard lock(mutex);
    auto& slot = modules[path];
    if(auto old = slot.lock()) return old;
    auto module = std::make_shared<Module>();
#ifdef _WIN32
    module->handle = LoadLibraryExW(path.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(module->handle) module->entry = reinterpret_cast<const clap_plugin_entry_t*>(GetProcAddress(module->handle, "clap_entry"));
#elif defined(__APPLE__)
    auto bytes = detail::utf8(path);
    auto url = CFURLCreateFromFileSystemRepresentation(nullptr,
        reinterpret_cast<const UInt8*>(bytes.data()), bytes.size(), true);
    if(url) { module->handle = CFBundleCreate(nullptr, url); CFRelease(url); }
    if(module->handle) module->entry = static_cast<const clap_plugin_entry_t*>(
        CFBundleGetDataPointerForName(module->handle, CFSTR("clap_entry")));
#else
    module->handle = dlopen(path.c_str(), RTLD_LOCAL | RTLD_NOW);
    if(module->handle) module->entry = static_cast<const clap_plugin_entry_t*>(dlsym(module->handle, "clap_entry"));
#endif
    auto* e = module->entry;
    if (!e || !e->init || !e->deinit || !e->get_factory || !clap_version_is_compatible(e->clap_version))
        throw std::runtime_error("Cannot load a compatible CLAP entry: " + detail::utf8(path));
    if (!e->init(detail::utf8(path).c_str())) throw std::runtime_error("CLAP entry initialization failed");
    module->initialized = true;
    slot = module;
    return module;
}
union NativeEvent {
    clap_event_header_t header;
    clap_event_param_value_t parameter;
    clap_event_midi_t midi;
    clap_event_note_t note;
};
static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
}
struct ClapPlugin::Impl {
    std::shared_ptr<Module> module;
    clap_host_t host{};
    const clap_plugin_t* plugin = nullptr;
    const clap_plugin_params_t* params = nullptr;
    const clap_plugin_state_t* state = nullptr;
    const clap_plugin_gui_t* gui=nullptr;
    EditorCallbacks editorCallbacks{};bool editorCreated=false;WindowApi editorApi=WindowApi::win32;
    std::atomic<bool> editorClosed{false};
    struct Timer{clap_id id=CLAP_INVALID_ID;std::uint32_t period=0;std::chrono::steady_clock::time_point next;};
    std::array<Timer,64> timers{};clap_id nextTimer=1;
    static bool CLAP_ABI registerTimer(const clap_host_t* h,std::uint32_t period,clap_id* id){
        auto& s=self(h);if(s.owner!=std::this_thread::get_id()||!period||!id)return false;
        for(auto& timer:s.timers)if(timer.id==CLAP_INVALID_ID){if(s.nextTimer==CLAP_INVALID_ID)s.nextTimer=1;timer={s.nextTimer++,period,std::chrono::steady_clock::now()+std::chrono::milliseconds(period)};*id=timer.id;return true;}return false;
    }
    static bool CLAP_ABI unregisterTimer(const clap_host_t* h,clap_id id){auto& s=self(h);if(s.owner!=std::this_thread::get_id())return false;for(auto& timer:s.timers)if(timer.id==id&&id!=CLAP_INVALID_ID){timer.id=CLAP_INVALID_ID;return true;}return false;}
    static const char* windowApi(WindowApi a){return a==WindowApi::win32?CLAP_WINDOW_API_WIN32:a==WindowApi::cocoa?CLAP_WINDOW_API_COCOA:CLAP_WINDOW_API_X11;}
    std::thread::id owner = std::this_thread::get_id();
    PluginDescriptor description;
    std::vector<AudioPort> inPorts, outPorts;
    std::vector<Parameter> parameters;
    std::unordered_map<std::uint32_t, std::size_t> parameterIndex;
    std::vector<std::uint32_t> noteDialects;
    std::vector<clap_audio_buffer_t> inputs, outputs;
    std::vector<NativeEvent> nativeEvents;
    std::atomic<std::uint32_t> requests{0};
    std::uint32_t maxFrames = 0, latency = 0, eventCount = 0;
    bool active = false;std::atomic<bool> processing{false};
    ProcessBlock* current = nullptr;

    void control() const {
        if(std::this_thread::get_id() != owner) throw std::logic_error("CLAP control operation must run on its creator thread");
    }
    void idle() const { control(); if(processing) throw std::logic_error("Stop audio before this operation"); }
    static Impl& self(const clap_host_t* h) { return *static_cast<Impl*>(h->host_data); }
    static void CLAP_ABI restart(const clap_host_t* h) { self(h).requests.fetch_or(restartRequested, std::memory_order_relaxed); }
    static void CLAP_ABI requestProcess(const clap_host_t* h) { self(h).requests.fetch_or(processRequested, std::memory_order_relaxed); }
    static void CLAP_ABI callback(const clap_host_t* h) { self(h).requests.fetch_or(callbackRequested, std::memory_order_relaxed); }
    static void CLAP_ABI rescan(const clap_host_t* h, clap_param_rescan_flags) { self(h).requests.fetch_or(parametersChanged, std::memory_order_relaxed); }
    static void CLAP_ABI clear(const clap_host_t* h, clap_id, clap_param_clear_flags) { self(h).requests.fetch_or(parametersChanged, std::memory_order_relaxed); }
    static void CLAP_ABI flush(const clap_host_t* h) { self(h).requests.fetch_or(flushRequested, std::memory_order_relaxed); }
    static void CLAP_ABI guiHints(const clap_host_t*){}
    static bool CLAP_ABI guiResize(const clap_host_t* h,std::uint32_t w,std::uint32_t v){auto& s=self(h);return s.owner==std::this_thread::get_id()&&w&&v&&w<=8192&&v<=8192&&s.editorCallbacks.requestResize&&s.editorCallbacks.requestResize(s.editorCallbacks.context,{w,v});}
    static bool CLAP_ABI guiShow(const clap_host_t*){return false;}
    static bool CLAP_ABI guiHide(const clap_host_t*){return false;}
    static void CLAP_ABI guiClosed(const clap_host_t* h,bool){self(h).editorClosed.store(true);callback(h);}
    static const void* CLAP_ABI extension(const clap_host_t*, const char* id) {
        static const clap_host_params_t p{rescan, clear, flush};
        static const clap_host_gui_t g{guiHints,guiResize,guiShow,guiHide,guiClosed};
        static const clap_host_timer_support_t t{registerTimer,unregisterTimer};
        if(id&&!std::strcmp(id,CLAP_EXT_TIMER_SUPPORT))return &t;
        if(id&&!std::strcmp(id,CLAP_EXT_GUI))return &g;
        return id && std::strcmp(id, CLAP_EXT_PARAMS) == 0 ? &p : nullptr;
    }
    static std::uint32_t CLAP_ABI size(const clap_input_events_t* list) {
        return static_cast<Impl*>(list->ctx)->eventCount;
    }
    static const clap_event_header_t* CLAP_ABI get(const clap_input_events_t* list, std::uint32_t index) {
        auto& s = *static_cast<Impl*>(list->ctx);
        return index < s.eventCount ? &s.nativeEvents[index].header : nullptr;
    }
    static bool CLAP_ABI push(const clap_output_events_t* list, const clap_event_header_t* header) {
        auto& s = *static_cast<Impl*>(list->ctx);
        if(!s.current){
            if(!header||header->space_id!=CLAP_CORE_EVENT_SPACE_ID||!s.editorCallbacks.parameterEdit)return false;
            if(header->type==CLAP_EVENT_PARAM_VALUE&&header->size>=sizeof(clap_event_param_value_t)){
                auto* e=reinterpret_cast<const clap_event_param_value_t*>(header);s.editorCallbacks.parameterEdit(s.editorCallbacks.context,e->param_id,e->value,EditGesture::value);return true;
            }
            if((header->type==CLAP_EVENT_PARAM_GESTURE_BEGIN||header->type==CLAP_EVENT_PARAM_GESTURE_END)&&header->size>=sizeof(clap_event_param_gesture_t)){
                auto* e=reinterpret_cast<const clap_event_param_gesture_t*>(header);s.editorCallbacks.parameterEdit(s.editorCallbacks.context,e->param_id,0,header->type==CLAP_EVENT_PARAM_GESTURE_BEGIN?EditGesture::begin:EditGesture::end);return true;
            }return false;
        }
        auto& b = *s.current;
        auto fail = [&] { b.outputEventsDropped = true; return false; };
        if (!header || header->space_id != CLAP_CORE_EVENT_SPACE_ID ||
            header->time >= b.frames || b.outputEventCount >= b.outputEvents.size()) return fail();
        if (b.outputEventCount && header->time < b.outputEvents[b.outputEventCount - 1].frame) return fail();
        Event event;
        event.frame = header->time;
        switch(header->type) {
        case CLAP_EVENT_PARAM_GESTURE_BEGIN: case CLAP_EVENT_PARAM_GESTURE_END: {
            if(header->size<sizeof(clap_event_param_gesture_t))return fail();
            event.kind=header->type==CLAP_EVENT_PARAM_GESTURE_BEGIN?EventKind::gestureBegin:EventKind::gestureEnd;
            event.parameterId=reinterpret_cast<const clap_event_param_gesture_t*>(header)->param_id;break;
        }
        case CLAP_EVENT_PARAM_VALUE: {
            if(header->size < sizeof(clap_event_param_value_t)) return fail();
            auto& e = *reinterpret_cast<const clap_event_param_value_t*>(header);
            if(e.note_id != -1 || e.port_index != -1 || e.channel != -1 || e.key != -1 || !std::isfinite(e.value)) return fail();
            event.kind = EventKind::parameter; event.parameterId = e.param_id; event.value = e.value;
            break;
        }
        case CLAP_EVENT_MIDI: {
            if(header->size < sizeof(clap_event_midi_t)) return fail();
            auto& e = *reinterpret_cast<const clap_event_midi_t*>(header);
            event.kind = EventKind::midi; event.port = static_cast<std::int16_t>(e.port_index);
            std::memcpy(event.midi, e.data, 3); break;
        }
        case CLAP_EVENT_NOTE_ON: case CLAP_EVENT_NOTE_OFF: {
            if(header->size < sizeof(clap_event_note_t)) return fail();
            auto& e = *reinterpret_cast<const clap_event_note_t*>(header);
            event.kind = header->type == CLAP_EVENT_NOTE_ON ? EventKind::noteOn : EventKind::noteOff;
            event.port = e.port_index; event.channel = e.channel; event.key = e.key;
            event.noteId = e.note_id; event.value = e.velocity; break;
        }
        default: return fail();
        }
        b.outputEvents[b.outputEventCount++] = event;
        return true;
    }
    void describe() {
        auto* d = plugin->desc;
        if(!d || !d->id || !d->name) throw std::runtime_error("Invalid CLAP plugin descriptor");
        description = {d->id, d->name, safe(d->vendor), safe(d->version)};
        auto* audio = static_cast<const clap_plugin_audio_ports_t*>(plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS));
        auto ports = [&](bool input, std::vector<AudioPort>& out) {
            auto n = audio ? audio->count(plugin, input) : 0;
            if(n > 128) throw std::runtime_error("Too many audio buses");
            for(std::uint32_t i = 0; i < n; ++i) {
                clap_audio_port_info_t info{};
                if(!audio->get(plugin, i, input, &info) || !info.channel_count || info.channel_count > 64)
                    throw std::runtime_error("Unsupported audio port (float32 required)");
                out.push_back({info.id, info.channel_count, info.name});
            }
        };
        ports(true, inPorts); ports(false, outPorts);
        params = static_cast<const clap_plugin_params_t*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));
        state = static_cast<const clap_plugin_state_t*>(plugin->get_extension(plugin, CLAP_EXT_STATE));
        gui = static_cast<const clap_plugin_gui_t*>(plugin->get_extension(plugin, CLAP_EXT_GUI));
        auto count = params ? params->count(plugin) : 0;
        if(count > 65536) throw std::runtime_error("Too many parameters");
        for(std::uint32_t i = 0; i < count; ++i) {
            clap_param_info_t p{};
            if(!params->get_info(plugin, i, &p)) throw std::runtime_error("Cannot describe CLAP parameter");
            if(!parameterIndex.emplace(p.id, parameters.size()).second) throw std::runtime_error("Duplicate parameter ID");
            parameters.push_back({p.id, p.name, p.min_value, p.max_value, p.default_value,
                bool(p.flags & CLAP_PARAM_IS_AUTOMATABLE), bool(p.flags & CLAP_PARAM_IS_STEPPED)});
        }
        auto* notes = static_cast<const clap_plugin_note_ports_t*>(plugin->get_extension(plugin, CLAP_EXT_NOTE_PORTS));
        auto noteCount = notes ? notes->count(plugin, true) : 0;
        if(noteCount > 128) throw std::runtime_error("Too many note ports");
        for(std::uint32_t i = 0; i < noteCount; ++i) {
            clap_note_port_info_t n{};
            if(!notes->get(plugin, i, true, &n)) throw std::runtime_error("Cannot describe CLAP note port");
            noteDialects.push_back(n.supported_dialects);
        }
    }
    ~Impl() {
        if(plugin) {
            if(editorCreated&&gui&&gui->destroy)gui->destroy(plugin);
            if(processing) plugin->stop_processing(plugin);
            if(active) plugin->deactivate(plugin);
            plugin->destroy(plugin);
        }
    }
};
ClapPlugin::ClapPlugin(const std::filesystem::path& path, const std::string& id) : impl_(std::make_unique<Impl>()) {
    auto& s = *impl_;
    s.module = loadModule(path);
    s.host = {CLAP_VERSION, &s, "CyCLAPBridge", "CyCLAPBridge", "", "0.1.0",
              Impl::extension, Impl::restart, Impl::requestProcess, Impl::callback};
    auto* factory = static_cast<const clap_plugin_factory_t*>(s.module->entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    if(!factory || !factory->get_plugin_count || !factory->get_plugin_descriptor || !factory->create_plugin)
        throw std::runtime_error("Missing CLAP plugin factory");
    auto count = factory->get_plugin_count(factory);
    if(count > 65536) throw std::runtime_error("Too many plugin descriptors");
    const clap_plugin_descriptor_t* descriptor = nullptr;
    for(std::uint32_t i = 0; i < count; ++i) {
        auto* candidate = factory->get_plugin_descriptor(factory, i);
        if(candidate && candidate->id && (id.empty() || id == candidate->id)) { descriptor = candidate; break; }
    }
    if(!descriptor) throw std::runtime_error("Requested CLAP plugin ID was not found");
    auto* p = factory->create_plugin(factory, &s.host, descriptor->id);
    if(!p) throw std::runtime_error("CLAP factory could not create plugin");
    if(!p->destroy) throw std::runtime_error("Invalid CLAP lifecycle table");
    s.plugin = p;
    if(!p->init || !p->activate || !p->deactivate || !p->start_processing || !p->stop_processing ||
       !p->process || !p->get_extension || !p->on_main_thread || !p->init(p))
        throw std::runtime_error("CLAP initialization failed or invalid lifecycle table");
    s.describe();
}
ClapPlugin::~ClapPlugin() = default;
const PluginDescriptor& ClapPlugin::descriptor() const noexcept { return impl_->description; }
const std::vector<AudioPort>& ClapPlugin::inputs() const noexcept { return impl_->inPorts; }
const std::vector<AudioPort>& ClapPlugin::outputs() const noexcept { return impl_->outPorts; }
const std::vector<Parameter>& ClapPlugin::parameters() const noexcept { return impl_->parameters; }
bool ClapPlugin::parameterValue(std::uint32_t id,double& value) const{auto& s=*impl_;s.control();return s.params&&s.params->get_value&&s.params->get_value(s.plugin,id,&value);}
void ClapPlugin::refresh() {
    auto& s = *impl_; s.idle();
    if(s.active) throw std::logic_error("Release before refreshing plugin metadata");
    s.inPorts.clear(); s.outPorts.clear(); s.parameters.clear();
    s.parameterIndex.clear(); s.noteDialects.clear();
    s.describe();
}
void ClapPlugin::prepare(double rate, std::uint32_t frames, std::uint32_t events) {
    auto& s = *impl_; s.idle();
    if(s.active) throw std::logic_error("Release before preparing again");
    if(!std::isfinite(rate) || rate < 1 || !frames || frames > 1048576 || events > 1048576)
        throw std::invalid_argument("Invalid preparation limits");
    s.inputs.resize(s.inPorts.size()); s.outputs.resize(s.outPorts.size()); s.nativeEvents.resize(events);
    if(!s.plugin->activate(s.plugin, rate, 1, frames)) throw std::runtime_error("CLAP activation failed");
    s.active = true; s.maxFrames = frames;
    auto* latency = static_cast<const clap_plugin_latency_t*>(s.plugin->get_extension(s.plugin, CLAP_EXT_LATENCY));
    s.latency = latency ? latency->get(s.plugin) : 0;
}
void ClapPlugin::release() {
    auto& s = *impl_; s.idle();
    if(s.active) s.plugin->deactivate(s.plugin);
    s.active = false; s.maxFrames = 0; s.latency = 0;
}
bool ClapPlugin::start() noexcept {
    auto& s = *impl_;
    if(!s.active || s.processing) return false;
    s.processing = s.plugin->start_processing(s.plugin);
    return s.processing;
}
void ClapPlugin::stop() noexcept {
    auto& s = *impl_;
    if(s.processing) s.plugin->stop_processing(s.plugin);
    s.processing = false;
}
ProcessStatus ClapPlugin::process(ProcessBlock& b) noexcept {
    auto& s = *impl_;
    b.outputEventCount = 0; b.outputEventsDropped = false;
    if(!s.processing) return ProcessStatus::notPrepared;
    if(!b.frames || b.frames > s.maxFrames || b.inputs.size() != s.inPorts.size() ||
       b.outputs.size() != s.outPorts.size() || b.events.size() > s.nativeEvents.size()) return ProcessStatus::invalidBlock;
    auto buses = [](auto incoming, auto& prepared, const auto& ports) {
        for(std::size_t i = 0; i < incoming.size(); ++i) {
            const auto& bus = incoming[i];
            if(bus.channelCount != ports[i].channels || !bus.channels) return false;
            for(std::uint32_t c = 0; c < bus.channelCount; ++c) if(!bus.channels[c]) return false;
            prepared[i] = {};
            prepared[i].data32 = const_cast<float**>(bus.channels);
            prepared[i].channel_count = bus.channelCount;
        }
        return true;
    };
    if(!buses(b.inputs, s.inputs, s.inPorts) || !buses(b.outputs, s.outputs, s.outPorts)) return ProcessStatus::invalidBlock;
    std::uint32_t last = 0;
    for(std::size_t i = 0; i < b.events.size(); ++i) {
        const auto& e = b.events[i];
        if(e.frame >= b.frames || e.frame < last || !std::isfinite(e.value)) return ProcessStatus::invalidBlock;
        last = e.frame;
        auto& n = s.nativeEvents[i]; n = {};
        if(e.kind == EventKind::parameter) {
            auto p = s.parameterIndex.find(e.parameterId);
            if(p == s.parameterIndex.end()) return ProcessStatus::invalidBlock;
            auto& info = s.parameters[p->second];
            if(e.value < info.minimum || e.value > info.maximum) return ProcessStatus::invalidBlock;
            n.parameter.header = {sizeof(clap_event_param_value_t), e.frame, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
            n.parameter.param_id = e.parameterId; n.parameter.value = e.value;
            n.parameter.note_id = n.parameter.port_index = n.parameter.channel = n.parameter.key = -1;
        } else {
            if(e.port < 0 || static_cast<std::size_t>(e.port) >= s.noteDialects.size()) return ProcessStatus::invalidBlock;
            if(e.kind == EventKind::midi) {
                if(!(s.noteDialects[e.port] & CLAP_NOTE_DIALECT_MIDI)) return ProcessStatus::invalidBlock;
                n.midi.header = {sizeof(clap_event_midi_t), e.frame, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_MIDI, 0};
                n.midi.port_index = static_cast<std::uint16_t>(e.port); std::memcpy(n.midi.data, e.midi, 3);
            } else if(e.kind == EventKind::noteOn || e.kind == EventKind::noteOff) {
                if(!(s.noteDialects[e.port] & CLAP_NOTE_DIALECT_CLAP) || e.channel < 0 || e.channel > 15 ||
                    e.key < 0 || e.key > 127 || e.noteId < -1 || e.value < 0 || e.value > 1)
                    return ProcessStatus::invalidBlock;
                n.note.header = {sizeof(clap_event_note_t), e.frame, CLAP_CORE_EVENT_SPACE_ID,
                    static_cast<std::uint16_t>(e.kind == EventKind::noteOn ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF), 0};
                n.note.port_index = e.port; n.note.channel = e.channel; n.note.key = e.key;
                n.note.note_id = e.noteId; n.note.velocity = e.value;
            } else return ProcessStatus::invalidBlock;
        }
    }
    s.eventCount = static_cast<std::uint32_t>(b.events.size());
    s.current = &b;
    clap_input_events_t in{&s, Impl::size, Impl::get};
    clap_output_events_t out{&s, Impl::push};
    clap_process_t process{};
    process.steady_time = -1; process.frames_count = b.frames;
    process.audio_inputs = s.inputs.data(); process.audio_outputs = s.outputs.data();
    process.audio_inputs_count = static_cast<std::uint32_t>(s.inputs.size());
    process.audio_outputs_count = static_cast<std::uint32_t>(s.outputs.size());
    process.in_events = &in; process.out_events = &out;
    auto result = s.plugin->process(s.plugin, &process);
    s.current = nullptr;
    return result == CLAP_PROCESS_ERROR ? ProcessStatus::pluginError : ProcessStatus::ok;
}
std::uint32_t ClapPlugin::latencyFrames() const noexcept { return impl_->latency; }
std::uint32_t ClapPlugin::pumpMainThread() {
    auto& s = *impl_; s.control();
    auto requests = s.requests.exchange(0, std::memory_order_relaxed);
    if(requests & callbackRequested) s.plugin->on_main_thread(s.plugin);
    // Active CLAP parameter flushes belong to the audio owner, including when
    // processing is stopped. The UI pump only flushes deactivated instances.
    if((requests&flushRequested)&&!s.active&&s.params){s.eventCount=0;clap_input_events_t in{&s,Impl::size,Impl::get};clap_output_events_t out{&s,Impl::push};s.params->flush(s.plugin,&in,&out);}
    if(s.editorClosed.exchange(false)&&s.editorCallbacks.closed)s.editorCallbacks.closed(s.editorCallbacks.context);
    auto* timerExtension=static_cast<const clap_plugin_timer_support_t*>(s.plugin->get_extension(s.plugin,CLAP_EXT_TIMER_SUPPORT));
    if(timerExtension&&timerExtension->on_timer){auto now=std::chrono::steady_clock::now();for(auto& timer:s.timers)if(timer.id!=CLAP_INVALID_ID&&now>=timer.next){timer.next=now+std::chrono::milliseconds(timer.period);timerExtension->on_timer(s.plugin,timer.id);}}
    return requests;
}
std::vector<std::byte> ClapPlugin::saveState(std::size_t maximum) {
    auto& s = *impl_; s.idle();
    if(!s.state) throw std::runtime_error("Plugin does not expose state");
    if(maximum > 64u * 1024u * 1024u) throw std::invalid_argument("State capacity exceeds 64 MiB");
    struct Writer { std::vector<std::byte> bytes; std::size_t limit; bool failed = false; } w{{}, maximum};
    clap_ostream_t stream{&w, [](const clap_ostream_t* out, const void* data, std::uint64_t count) -> std::int64_t {
        auto& writer = *static_cast<Writer*>(out->ctx);
        if(count > writer.limit - writer.bytes.size() || (!data && count)) { writer.failed = true; return -1; }
        if(!count) return 0;
        try {
            auto* start = static_cast<const std::byte*>(data);
            writer.bytes.insert(writer.bytes.end(), start, start + static_cast<std::size_t>(count));
        } catch (...) { writer.failed = true; return -1; }
        return static_cast<std::int64_t>(count);
    }};
    if(!s.state->save(s.plugin, &stream) || w.failed) throw std::runtime_error("CLAP state save failed or exceeded capacity");
    return std::move(w.bytes);
}
void ClapPlugin::loadState(std::span<const std::byte> bytes) {
    auto& s = *impl_; s.idle();
    if(!s.state) throw std::runtime_error("Plugin does not expose state");
    if(bytes.size() > 64u * 1024u * 1024u) throw std::invalid_argument("State exceeds 64 MiB");
    struct Reader { std::span<const std::byte> bytes; std::size_t offset = 0; } r{bytes};
    clap_istream_t stream{&r, [](const clap_istream_t* in, void* data, std::uint64_t count) -> std::int64_t {
        auto& reader = *static_cast<Reader*>(in->ctx);
        if(!data && count) return -1;
        auto n = std::min<std::uint64_t>(count, reader.bytes.size() - reader.offset);
        if(n) std::memcpy(data, reader.bytes.data() + reader.offset, static_cast<std::size_t>(n));
        reader.offset += static_cast<std::size_t>(n);
        return static_cast<std::int64_t>(n);
    }};
    if(!s.state->load(s.plugin, &stream)) throw std::runtime_error("CLAP state load failed; plugin may have partially changed");
}
bool ClapPlugin::editorSupported(WindowApi a) const{auto& s=*impl_;s.control();return s.gui&&s.gui->is_api_supported&&s.gui->is_api_supported(s.plugin,Impl::windowApi(a),false);}
bool ClapPlugin::createEditor(WindowApi a,const EditorCallbacks& c){auto& s=*impl_;s.control();if(s.editorCreated||!editorSupported(a)||!s.gui->create)return false;s.editorCallbacks=c;s.editorApi=a;s.editorCreated=s.gui->create(s.plugin,Impl::windowApi(a),false);if(!s.editorCreated)s.editorCallbacks={};return s.editorCreated;}
void ClapPlugin::destroyEditor(){auto& s=*impl_;s.control();if(s.editorCreated&&s.gui->destroy)s.gui->destroy(s.plugin);s.editorCreated=false;s.editorCallbacks={};}
EditorSize ClapPlugin::editorSize() const{auto& s=*impl_;s.control();EditorSize size{};if(s.editorCreated&&s.gui->get_size)s.gui->get_size(s.plugin,&size.width,&size.height);return size;}
bool ClapPlugin::editorCanResize() const{auto& s=*impl_;s.control();return s.editorCreated&&s.gui->can_resize&&s.gui->can_resize(s.plugin);}
bool ClapPlugin::adjustEditorSize(EditorSize& size) const{auto& s=*impl_;s.control();return s.editorCreated&&s.gui->adjust_size&&s.gui->adjust_size(s.plugin,&size.width,&size.height);}
bool ClapPlugin::resizeEditor(EditorSize size){auto& s=*impl_;s.control();return s.editorCreated&&size.width&&size.height&&size.width<=8192&&size.height<=8192&&s.gui->set_size&&s.gui->set_size(s.plugin,size.width,size.height);}
bool ClapPlugin::setEditorParent(NativeWindow p){auto& s=*impl_;s.control();if(!s.editorCreated||p.api!=s.editorApi||!p.handle||!s.gui->set_parent)return false;clap_window_t w{};w.api=Impl::windowApi(p.api);w.ptr=p.handle;return s.gui->set_parent(s.plugin,&w);}
bool ClapPlugin::setEditorScale(double v){auto& s=*impl_;s.control();return s.editorCreated&&std::isfinite(v)&&v>0&&s.gui->set_scale&&s.gui->set_scale(s.plugin,v);}
bool ClapPlugin::showEditor(bool v){auto& s=*impl_;s.control();if(!s.editorCreated)return false;auto call=v?s.gui->show:s.gui->hide;return call&&call(s.plugin);}
}
