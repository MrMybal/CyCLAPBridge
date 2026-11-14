// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>
#include <cyclapbridge/Conversion.h>
#include <cyclapbridge/Editor.h>
namespace cybridge {
struct PluginDescriptor { std::string id, name, vendor, version; };
struct AudioPort { std::uint32_t id, channels; std::string name; };
struct Parameter {
    std::uint32_t id;
    std::string name;
    double minimum, maximum, defaultValue;
    bool automatable, stepped;
};
enum class EventKind { parameter, midi, noteOn, noteOff, gestureBegin, gestureEnd };
struct Event {
    EventKind kind = EventKind::parameter;
    std::uint32_t frame = 0, parameterId = 0;
    double value = 0;
    std::int16_t port = 0, channel = 0, key = 60;
    std::int32_t noteId = -1;
    std::uint8_t midi[3]{};
};
struct AudioBus { float* const* channels = nullptr; std::uint32_t channelCount = 0; };
struct ProcessBlock {
    std::uint32_t frames = 0;
    std::span<const AudioBus> inputs, outputs;
    std::span<const Event> events;
    std::span<Event> outputEvents;
    std::size_t outputEventCount = 0;
    bool outputEventsDropped = false;
};
enum class ProcessStatus { ok, invalidBlock, notPrepared, pluginError };
enum HostRequest : std::uint32_t {
    restartRequested = 1, processRequested = 2, callbackRequested = 4,
    parametersChanged = 8, flushRequested = 16
};
// Direct CLAP hosting inside the process selected by the DAW (main or worker).
// The DAW owns process isolation, IPC, crash recovery and security sandboxing.
// Instances, SDK handles and ProcessBlock views remain local to that process.
// Control/lifecycle: creator/main thread. start/process/stop: one audio owner.
// Stop before changing/destroying. Prepared host code uses bounded storage;
// arbitrary foreign plugin code retains its own behavior.
class ClapPlugin final {
public:
    explicit ClapPlugin(const std::filesystem::path& absoluteModule, const std::string& pluginId = {});
    ~ClapPlugin();
    ClapPlugin(const ClapPlugin&) = delete;
    ClapPlugin& operator=(const ClapPlugin&) = delete;
    const PluginDescriptor& descriptor() const noexcept;
    const std::vector<AudioPort>& inputs() const noexcept;
    const std::vector<AudioPort>& outputs() const noexcept;
    const std::vector<Parameter>& parameters() const noexcept;
    // Creator/UI thread only; the backend may return its published audio value.
    bool parameterValue(std::uint32_t id,double& value) const;
    // Re-read ports and parameters while deactivated (after a restart/rescan).
    void refresh();
    void prepare(double sampleRate, std::uint32_t maximumFrames, std::uint32_t maximumEvents = 1024);
    void release();
    bool start() noexcept;
    void stop() noexcept;
    ProcessStatus process(ProcessBlock&) noexcept;
    std::uint32_t latencyFrames() const noexcept;
    std::uint32_t pumpMainThread();
    std::vector<std::byte> saveState(std::size_t maximumBytes = 64u * 1024u * 1024u);
    void loadState(std::span<const std::byte>);
    bool editorSupported(WindowApi) const;
    bool createEditor(WindowApi,const EditorCallbacks& = {});
    void destroyEditor();
    EditorSize editorSize() const;
    bool editorCanResize() const;
    bool adjustEditorSize(EditorSize&) const;
    bool resizeEditor(EditorSize);
    bool setEditorParent(NativeWindow);
    bool setEditorScale(double);
    bool showEditor(bool visible);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
// Format-neutral direct hosting. No audio device or UI toolkit is imported.
// Construct inside the DAW-selected process; process management belongs to the DAW.
// Same thread/ownership contract as ClapPlugin. Parameters keep native units;
// VST3 parameters use normalized [0,1] values.
class HostedPlugin final {
public:
    HostedPlugin(Format, const std::filesystem::path&, const std::string& pluginId = {});
    ~HostedPlugin();
    HostedPlugin(const HostedPlugin&) = delete;
    HostedPlugin& operator=(const HostedPlugin&) = delete;
    const PluginDescriptor& descriptor() const noexcept;
    const std::vector<AudioPort>& inputs() const noexcept;
    const std::vector<AudioPort>& outputs() const noexcept;
    const std::vector<Parameter>& parameters() const noexcept;
    // Creator/UI thread only; the backend may return its published audio value.
    bool parameterValue(std::uint32_t id,double& value) const;
    void prepare(double, std::uint32_t, std::uint32_t = 1024);
    void release();
    bool start() noexcept;
    void stop() noexcept;
    ProcessStatus process(ProcessBlock&) noexcept;
    std::uint32_t latencyFrames() const noexcept;
    std::uint32_t pumpMainThread();
    std::vector<std::byte> saveState(std::size_t = 64u*1024u*1024u);
    void loadState(std::span<const std::byte>);
    // Embedded editor only, on the creator/UI thread. Pump at about 30 Hz while
    // open for LV2 idle, JSFX @gfx and host-to-editor parameter synchronization.
    // The DAW owns the native parent and destroys the editor before the parent.
    bool editorSupported(WindowApi) const;
    bool createEditor(WindowApi,const EditorCallbacks& = {});
    void destroyEditor();
    EditorSize editorSize() const;
    bool editorCanResize() const;
    bool adjustEditorSize(EditorSize&) const;
    bool resizeEditor(EditorSize);
    bool setEditorParent(NativeWindow);
    bool setEditorScale(double);
    bool showEditor(bool visible);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
