// SPDX-License-Identifier: MIT
// AUv2 hosting through public Apple APIs. Compiled only on macOS.
#include "HostedBackend.h"
#include "Process.h"
#include "AuEditor.h"
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <algorithm>
#include <cstring>
#include <sstream>
namespace cybridge::detail {
namespace {
std::string string(CFStringRef value){
    if(!value)return {};char bytes[4096]{};if(!CFStringGetCString(value,bytes,sizeof(bytes),kCFStringEncodingUTF8))return {};return bytes;
}
OSType code(CFStringRef value){
    auto s=string(value);if(s.size()!=4)throw std::runtime_error("Invalid AU component four-character code");
    return (static_cast<std::uint32_t>(s[0])<<24)|(static_cast<std::uint32_t>(s[1])<<16)|(static_cast<std::uint32_t>(s[2])<<8)|static_cast<std::uint32_t>(s[3]);
}
struct Au final:HostedBackend {
    CFBundleRef bundle=nullptr;AudioUnit unit=nullptr;
    ProcessBlock* current=nullptr;std::uint32_t slice=0;
    double sampleTime=0;
    struct Buffer {std::uint32_t count;AudioBuffer data[64];};
    std::vector<Buffer> buffers;
    std::vector<AudioUnitParameterID> ids;
    std::unique_ptr<AuEditor> editor;
    double currentParameter(std::uint32_t id) const override{AudioUnitParameterValue value=0;AudioUnitGetParameter(unit,id,kAudioUnitScope_Global,0,&value);return value;}
    bool editorSupported(WindowApi api) const override{return api==WindowApi::cocoa&&hasAuEditor(unit);}
    bool createEditor(WindowApi api,const EditorCallbacks& callbacks) override{
        if(editor||!editorSupported(api))return false;editorCallbacks=callbacks;
        auto proxy=callbacks;proxy.context=this;
        proxy.requestResize=[](void* p,EditorSize size) noexcept{auto& s=*static_cast<Au*>(p);return s.editorCallbacks.requestResize&&s.editorCallbacks.requestResize(s.editorCallbacks.context,size);};
        proxy.parameterEdit=[](void* p,std::uint32_t id,double value,EditGesture gesture) noexcept{auto& s=*static_cast<Au*>(p);if(gesture==EditGesture::value)for(std::size_t i=0;i<s.params.size();++i)if(s.params[i].id==id&&s.mirror[i].value.load()==value)return;s.edited(id,value,gesture);};
        proxy.closed=nullptr;editor=makeAuEditor(unit,params,proxy);return bool(editor);
    }
    void destroyEditor() override{editor.reset();editorCallbacks={};}
    EditorSize editorSize() const override{return editor?editor->size():EditorSize{};}
    bool editorCanResize() const override{return editor&&editor->canResize();}
    bool adjustEditorSize(EditorSize& size) const override{return editorCanResize()&&validSize(size);}
    bool resizeEditor(EditorSize size) override{return editor&&editor->resize(size);}
    bool setEditorParent(NativeWindow parent) override{return editor&&parent.api==WindowApi::cocoa&&editor->parent(parent.handle);}
    bool showEditor(bool visible) override{return editor&&editor->show(visible);}
    void editorParameter(std::uint32_t id,double) override{if(editor)editor->parameter(id);}
    static OSStatus pull(void* context,AudioUnitRenderActionFlags*,const AudioTimeStamp*,UInt32 bus,UInt32 frames,AudioBufferList* data){
        auto& s=*static_cast<Au*>(context);
        if(!s.current||bus>=s.current->inputs.size())return kAudio_ParamError;
        const auto& input=s.current->inputs[bus];
        if(data->mNumberBuffers!=input.channelCount)return kAudio_ParamError;
        for(std::uint32_t c=0;c<input.channelCount;++c){data->mBuffers[c].mNumberChannels=1;data->mBuffers[c].mDataByteSize=frames*4;data->mBuffers[c].mData=input.channels[c]+s.slice;}
        return noErr;
    }
    Au(const std::filesystem::path& path,const std::string& id){
        try{
            auto bytes=utf8(path);auto url=CFURLCreateFromFileSystemRepresentation(nullptr,reinterpret_cast<const UInt8*>(bytes.data()),bytes.size(),true);
            if(url){bundle=CFBundleCreate(nullptr,url);CFRelease(url);}
            if(!bundle||!CFBundleLoadExecutable(bundle))throw std::runtime_error("Cannot load AU component bundle");
            auto* array=static_cast<CFArrayRef>(CFBundleGetValueForInfoDictionaryKey(bundle,CFSTR("AudioComponents")));
            if(!array||CFGetTypeID(array)!=CFArrayGetTypeID())throw std::runtime_error("AU bundle lacks AudioComponents metadata");
            AudioComponent selected=nullptr;
            for(CFIndex i=0;i<CFArrayGetCount(array);++i){
                auto dict=static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(array,i));
                auto get=[&](CFStringRef key){return static_cast<CFStringRef>(CFDictionaryGetValue(dict,key));};
                AudioComponentDescription desc{code(get(CFSTR("type"))),code(get(CFSTR("subtype"))),code(get(CFSTR("manufacturer"))),0,0};
                auto componentId=string(get(CFSTR("type")))+":"+string(get(CFSTR("subtype")))+":"+string(get(CFSTR("manufacturer")));
                if(!id.empty()&&id!=componentId)continue;
                auto factoryName=get(CFSTR("factoryFunction"));
                auto factory=reinterpret_cast<AudioComponentFactoryFunction>(CFBundleGetFunctionPointerForName(bundle,factoryName));
                if(!factory)throw std::runtime_error("AU factory symbol is absent");
                auto title=get(CFSTR("name"));selected=AudioComponentRegister(&desc,factory,title,0x00020000);
                info={componentId,string(title),"Audio Unit","1"};break;
            }
            if(!selected||AudioComponentInstanceNew(selected,&unit)!=noErr)throw std::runtime_error("AU component creation failed");
            for(auto scope:{kAudioUnitScope_Input,kAudioUnitScope_Output}){
                UInt32 count=0,size=sizeof(count);
                if(AudioUnitGetProperty(unit,kAudioUnitProperty_ElementCount,scope,0,&count,&size)!=noErr)continue;
                if(count>128)throw std::runtime_error("AU bus limit");
                auto& ports=scope==kAudioUnitScope_Input?ins:outs;
                for(UInt32 bus=0;bus<count;++bus){
                    AudioStreamBasicDescription format{};size=sizeof(format);
                    if(AudioUnitGetProperty(unit,kAudioUnitProperty_StreamFormat,scope,bus,&format,&size)!=noErr||!format.mChannelsPerFrame||format.mChannelsPerFrame>64)throw std::runtime_error("Unsupported AU stream format");
                    ports.push_back({bus,format.mChannelsPerFrame,scope==kAudioUnitScope_Input?"AU input":"AU output"});
                }
            }
            UInt32 bytesSize=0;Boolean writable=false;
            if(AudioUnitGetPropertyInfo(unit,kAudioUnitProperty_ParameterList,kAudioUnitScope_Global,0,&bytesSize,&writable)==noErr){
                if(bytesSize>65536*sizeof(AudioUnitParameterID))throw std::runtime_error("AU parameter limit");
                ids.resize(bytesSize/sizeof(AudioUnitParameterID));
                AudioUnitGetProperty(unit,kAudioUnitProperty_ParameterList,kAudioUnitScope_Global,0,ids.data(),&bytesSize);
                for(auto id:ids){
                    AudioUnitParameterInfo p{};bytesSize=sizeof(p);
                    if(AudioUnitGetProperty(unit,kAudioUnitProperty_ParameterInfo,kAudioUnitScope_Global,id,&p,&bytesSize)!=noErr)continue;
                    auto name=(p.flags&kAudioUnitParameterFlag_HasCFNameString)?string(p.cfNameString):std::string(p.name);
                    params.push_back({id,name,p.minValue,p.maxValue,p.defaultValue,bool(p.flags&kAudioUnitParameterFlag_IsWritable),false});
                    if((p.flags&kAudioUnitParameterFlag_CFNameRelease)&&p.cfNameString)CFRelease(p.cfNameString);
                }
            }
        }catch(...){if(unit)AudioComponentInstanceDispose(unit);if(bundle)CFRelease(bundle);throw;}
    }
    ~Au() override{destroyEditor();if(unit){if(prepared)AudioUnitUninitialize(unit);AudioComponentInstanceDispose(unit);}if(bundle)CFRelease(bundle);}
    void prepare(double rate,std::uint32_t f,std::uint32_t e) override{
        limits(rate,f,e);
        UInt32 frames=f;if(AudioUnitSetProperty(unit,kAudioUnitProperty_MaximumFramesPerSlice,kAudioUnitScope_Global,0,&frames,sizeof(frames))!=noErr)throw std::runtime_error("AU maximum slice setup failed");
        for(auto scope:{kAudioUnitScope_Input,kAudioUnitScope_Output}){
            auto& ports=scope==kAudioUnitScope_Input?ins:outs;
            for(const auto& p:ports){
                AudioStreamBasicDescription format{rate,kAudioFormatLinearPCM,kAudioFormatFlagIsFloat|kAudioFormatFlagIsPacked|kAudioFormatFlagIsNonInterleaved,4,1,4,p.channels,32,0};
                if(AudioUnitSetProperty(unit,kAudioUnitProperty_StreamFormat,scope,p.id,&format,sizeof(format))!=noErr)throw std::runtime_error("AU float32 format rejected");
                if(scope==kAudioUnitScope_Input){AURenderCallbackStruct callback{pull,this};if(AudioUnitSetProperty(unit,kAudioUnitProperty_SetRenderCallback,kAudioUnitScope_Input,p.id,&callback,sizeof(callback))!=noErr)throw std::runtime_error("AU render callback setup failed");}
            }
        }
        buffers.resize(outs.size());
        if(AudioUnitInitialize(unit)!=noErr)throw std::runtime_error("AU initialization failed");
        Float64 seconds=0;UInt32 size=sizeof(seconds);AudioUnitGetProperty(unit,kAudioUnitProperty_Latency,kAudioUnitScope_Global,0,&seconds,&size);
        latency=static_cast<std::uint32_t>(std::max(0.0,seconds)*rate);sampleTime=0;prepared=true;
    }
    void unprepare() override{idle();if(prepared)AudioUnitUninitialize(unit);prepared=false;}
    ProcessStatus process(ProcessBlock& block) noexcept override{
        if(!valid(block))return running?ProcessStatus::invalidBlock:ProcessStatus::notPrepared;
        for(const auto& e:block.events){
            if(e.kind==EventKind::parameter){if(AudioUnitSetParameter(unit,e.parameterId,kAudioUnitScope_Global,0,static_cast<AudioUnitParameterValue>(e.value),e.frame)!=noErr)return ProcessStatus::pluginError;}
            else {
                std::uint8_t data[3];std::memcpy(data,e.midi,3);
                if(e.kind!=EventKind::midi){data[0]=static_cast<std::uint8_t>((e.kind==EventKind::noteOn?0x90:0x80)|e.channel);data[1]=static_cast<std::uint8_t>(e.key);data[2]=static_cast<std::uint8_t>(e.value*127);}
                if(MusicDeviceMIDIEvent(unit,data[0],data[1],data[2],e.frame)!=noErr)return ProcessStatus::pluginError;
            }
        }
        current=&block;slice=0;AudioTimeStamp time{};time.mFlags=kAudioTimeStampSampleTimeValid;time.mSampleTime=sampleTime;
        for(std::size_t i=0;i<outs.size();++i){
            auto& b=buffers[i];b.count=outs[i].channels;
            for(std::uint32_t c=0;c<b.count;++c)b.data[c]={1,block.frames*4,block.outputs[i].channels[c]};
            AudioUnitRenderActionFlags flags=0;
            if(AudioUnitRender(unit,&flags,&time,static_cast<UInt32>(i),block.frames,reinterpret_cast<AudioBufferList*>(&b))!=noErr){current=nullptr;return ProcessStatus::pluginError;}
        }
        current=nullptr;sampleTime+=block.frames;return ProcessStatus::ok;
    }
    std::vector<std::byte> save(std::size_t limit) override{
        idle();CFPropertyListRef state=nullptr;UInt32 size=sizeof(state);
        if(AudioUnitGetProperty(unit,kAudioUnitProperty_ClassInfo,kAudioUnitScope_Global,0,&state,&size)!=noErr||!state)throw std::runtime_error("AU state unavailable");
        auto data=CFPropertyListCreateData(nullptr,state,kCFPropertyListBinaryFormat_v1_0,0,nullptr);CFRelease(state);
        if(!data)throw std::runtime_error("AU state serialization failed");
        auto n=CFDataGetLength(data);if(static_cast<std::size_t>(n)>limit){CFRelease(data);throw std::runtime_error("AU state capacity exceeded");}
        std::vector<std::byte> bytes(n);std::memcpy(bytes.data(),CFDataGetBytePtr(data),n);CFRelease(data);return bytes;
    }
    void load(std::span<const std::byte> bytes) override{
        idle();auto data=CFDataCreate(nullptr,reinterpret_cast<const UInt8*>(bytes.data()),bytes.size());
        auto state=CFPropertyListCreateWithData(nullptr,data,kCFPropertyListImmutable,nullptr,nullptr);CFRelease(data);
        if(!state)throw std::runtime_error("Invalid AU state property list");
        auto code=AudioUnitSetProperty(unit,kAudioUnitProperty_ClassInfo,kAudioUnitScope_Global,0,&state,sizeof(state));CFRelease(state);
        if(code!=noErr)throw std::runtime_error("AU state restore failed");
    }
};
}
std::unique_ptr<HostedBackend> makeAu(const std::filesystem::path& p,const std::string& id){return std::make_unique<Au>(p,id);}
}
