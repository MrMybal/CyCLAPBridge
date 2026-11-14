// SPDX-License-Identifier: MIT
// Apple-only source integration. Native macOS validation is deferred.
#import <Cocoa/Cocoa.h>
#import <AudioUnit/AUCocoaUIView.h>
#include "AuEditor.h"
#include <cstddef>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace cybridge::detail {
bool hasAuEditor(AudioUnit unit){UInt32 bytes=0;Boolean writable=false;return AudioUnitGetPropertyInfo(unit,kAudioUnitProperty_CocoaUI,kAudioUnitScope_Global,0,&bytes,&writable)==noErr&&bytes>=offsetof(AudioUnitCocoaViewInfo,mCocoaAUViewClass)+sizeof(CFStringRef)&&bytes<=65536;}
namespace {
class CocoaEditor final:public AuEditor {
    AudioUnit unit;EditorCallbacks callbacks;
    __strong NSBundle* bundle=nil;
    __strong id<AUCocoaUIBase> factory=nil;
    __strong NSView* view=nil;
    __strong NSView* container=nil;
    __strong id observer=nil;
    struct Listener{AUEventListenerRef handle=nullptr;~Listener(){if(handle)AUListenerDispose(handle);}} listener;
    bool changingSize=false;EditorSize dimensions{};
    static void event(void* data,void*,const AudioUnitEvent* incoming,UInt64,AudioUnitParameterValue value){
        auto& s=*static_cast<CocoaEditor*>(data);if(!incoming||!s.callbacks.parameterEdit)return;
        auto gesture=EditGesture::value;
        if(incoming->mEventType==kAudioUnitEvent_BeginParameterChangeGesture)gesture=EditGesture::begin;
        else if(incoming->mEventType==kAudioUnitEvent_EndParameterChangeGesture)gesture=EditGesture::end;
        else if(incoming->mEventType!=kAudioUnitEvent_ParameterValueChange)return;
        auto& parameter=incoming->mArgument.mParameter;
        if(gesture!=EditGesture::value)AudioUnitGetParameter(s.unit,parameter.mParameterID,parameter.mScope,parameter.mElement,&value);
        s.callbacks.parameterEdit(s.callbacks.context,parameter.mParameterID,value,gesture);
    }
    void viewResized(){
        if(changingSize)return;auto frame=view.frame;EditorSize requested{static_cast<std::uint32_t>(std::max(1.0,static_cast<double>(frame.size.width))),static_cast<std::uint32_t>(std::max(1.0,static_cast<double>(frame.size.height)))};
        if(requested.width==dimensions.width&&requested.height==dimensions.height)return;
        changingSize=true;
        if(requested.width<=8192&&requested.height<=8192&&callbacks.requestResize&&callbacks.requestResize(callbacks.context,requested)){dimensions=requested;[container setFrameSize:frame.size];}
        else [view setFrameSize:NSMakeSize(dimensions.width,dimensions.height)];
        changingSize=false;
    }
public:
    CocoaEditor(AudioUnit audioUnit,NSView* editor,NSBundle* loaded,id<AUCocoaUIBase> provider,const std::vector<Parameter>& parameters,EditorCallbacks c):unit(audioUnit),callbacks(c),bundle(loaded),factory(provider),view(editor){
        auto frame=view.frame;dimensions={static_cast<std::uint32_t>(std::max(1.0,static_cast<double>(frame.size.width))),static_cast<std::uint32_t>(std::max(1.0,static_cast<double>(frame.size.height)))};
        container=[[NSView alloc] initWithFrame:NSMakeRect(0,0,dimensions.width,dimensions.height)];[view setFrameOrigin:NSZeroPoint];[container addSubview:view];container.hidden=YES;
        view.postsFrameChangedNotifications=YES;
        if(AUEventListenerCreate(event,this,CFRunLoopGetMain(),kCFRunLoopCommonModes,.02f,.02f,&listener.handle)!=noErr)throw std::runtime_error("AU editor parameter listener unavailable");
        for(auto& p:parameters)for(auto type:{kAudioUnitEvent_ParameterValueChange,kAudioUnitEvent_BeginParameterChangeGesture,kAudioUnitEvent_EndParameterChangeGesture}){AudioUnitEvent e{};e.mEventType=type;e.mArgument.mParameter={unit,p.id,kAudioUnitScope_Global,0};AUEventListenerAddEventType(listener.handle,this,&e);}
        auto* self=this;observer=[[NSNotificationCenter defaultCenter] addObserverForName:NSViewFrameDidChangeNotification object:view queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification*){self->viewResized();}];
    }
    ~CocoaEditor() override {if(observer)[[NSNotificationCenter defaultCenter] removeObserver:observer];[view removeFromSuperview];[container removeFromSuperview];}
    EditorSize size() const override{return dimensions;}
    bool canResize() const override{return (view.autoresizingMask&(NSViewWidthSizable|NSViewHeightSizable))==(NSViewWidthSizable|NSViewHeightSizable);}
    bool resize(EditorSize size) override{if(!canResize())return false;changingSize=true;dimensions=size;auto value=NSMakeSize(size.width,size.height);[container setFrameSize:value];[view setFrameSize:value];changingSize=false;return true;}
    bool parent(void* handle) override{if(!handle||container.superview)return false;NSView* parent=(__bridge NSView*)handle;[parent addSubview:container];return true;}
    bool show(bool visible) override{if(!container.superview)return false;container.hidden=!visible;return true;}
    void parameter(std::uint32_t id) override{AudioUnitParameter p{unit,id,kAudioUnitScope_Global,0};AUParameterListenerNotify(nullptr,nullptr,&p);}
};
}
std::unique_ptr<AuEditor> makeAuEditor(AudioUnit unit,const std::vector<Parameter>& parameters,EditorCallbacks callbacks){
    if(!hasAuEditor(unit)||![NSThread isMainThread])return {};
    UInt32 bytes=0;Boolean writable=false;AudioUnitGetPropertyInfo(unit,kAudioUnitProperty_CocoaUI,kAudioUnitScope_Global,0,&bytes,&writable);
    std::vector<std::byte> storage(bytes);auto* info=reinterpret_cast<AudioUnitCocoaViewInfo*>(storage.data());
    if(AudioUnitGetProperty(unit,kAudioUnitProperty_CocoaUI,kAudioUnitScope_Global,0,info,&bytes)!=noErr)return {};
    struct Notices{AudioUnitCocoaViewInfo* info;UInt32 bytes;~Notices(){auto count=(bytes - offsetof(AudioUnitCocoaViewInfo,mCocoaAUViewClass))/sizeof(CFStringRef);for(std::size_t i=0;i<count;++i)if(info->mCocoaAUViewClass[i])CFRelease(info->mCocoaAUViewClass[i]);if(info->mCocoaAUViewBundleLocation)CFRelease(info->mCocoaAUViewBundleLocation);}} releases{info,bytes};
    @try{
        NSBundle* bundle=[NSBundle bundleWithURL:(__bridge NSURL*)info->mCocoaAUViewBundleLocation];
        if(!bundle||![bundle load])return {};
        Class type=[bundle classNamed:(__bridge NSString*)info->mCocoaAUViewClass[0]];
        if(!type||![type conformsToProtocol:@protocol(AUCocoaUIBase)])return {};
        id<AUCocoaUIBase> factory=[[type alloc] init];NSView* view=[factory uiViewForAudioUnit:unit withSize:NSMakeSize(640,480)];
        if(!view)return {};return std::make_unique<CocoaEditor>(unit,view,bundle,factory,parameters,callbacks);
    }@catch(NSException* exception){(void)exception;return {};}
}
}
