// SPDX-License-Identifier: MIT
// Included inside Instance. GUI callbacks run on the creator/UI thread.
bool editorCreated=false;clap_id editorTimer=CLAP_INVALID_ID;
struct UiEdit{std::uint32_t id;double value;cybridge::EditGesture gesture;};
std::array<UiEdit,1024> uiEdits{};std::atomic<std::uint32_t> uiRead{0},uiWrite{0};
std::atomic<bool> uiOverflow{false};
const clap_host_timer_support_t* hostTimer() const{return host->get_extension?static_cast<const clap_host_timer_support_t*>(host->get_extension(host,CLAP_EXT_TIMER_SUPPORT)):nullptr;}
static bool windowApi(const char* text,cybridge::WindowApi& api){
    if(!text)return false;
    if(!std::strcmp(text,CLAP_WINDOW_API_WIN32)){api=cybridge::WindowApi::win32;return true;}
    if(!std::strcmp(text,CLAP_WINDOW_API_COCOA)){api=cybridge::WindowApi::cocoa;return true;}
    if(!std::strcmp(text,CLAP_WINDOW_API_X11)){api=cybridge::WindowApi::x11;return true;}return false;
}
static bool resized(void* context,cybridge::EditorSize size) noexcept{
    auto& s=*static_cast<Instance*>(context);auto* gui=static_cast<const clap_host_gui_t*>(s.host->get_extension(s.host,CLAP_EXT_GUI));
    return gui&&gui->request_resize&&gui->request_resize(s.host,size.width,size.height);
}
static void edited(void* context,std::uint32_t id,double value,cybridge::EditGesture gesture) noexcept{
    auto& s=*static_cast<Instance*>(context);
    if(gesture==cybridge::EditGesture::value)for(std::size_t i=0;i<s.native->parameters().size();++i)if(s.native->parameters()[i].id==id){s.values[i].store(value);s.dirty[i].store(true);break;}
    auto write=s.uiWrite.load(std::memory_order_relaxed);auto next=static_cast<std::uint32_t>((write+1)%s.uiEdits.size());
    if(next==s.uiRead.load(std::memory_order_acquire))s.uiOverflow.store(true);
    else{s.uiEdits[write]={id,value,gesture};s.uiWrite.store(next,std::memory_order_release);}
    auto* params=static_cast<const clap_host_params_t*>(s.host->get_extension(s.host,CLAP_EXT_PARAMS));
    if(params&&params->request_flush)params->request_flush(s.host);
    s.host->request_process(s.host);
}
static void closed(void* context) noexcept{auto& s=*static_cast<Instance*>(context);auto* gui=static_cast<const clap_host_gui_t*>(s.host->get_extension(s.host,CLAP_EXT_GUI));if(gui&&gui->closed)gui->closed(s.host,false);}
void outputUiEdits(const clap_output_events_t* output){
    if(!output)return;auto read=uiRead.load(std::memory_order_relaxed),end=uiWrite.load(std::memory_order_acquire);
    while(read!=end){auto& edit=uiEdits[read];bool pushed=false;
        if(edit.gesture==cybridge::EditGesture::value){clap_event_param_value_t e{};e.header={sizeof(e),0,CLAP_CORE_EVENT_SPACE_ID,CLAP_EVENT_PARAM_VALUE,CLAP_EVENT_IS_LIVE};e.param_id=edit.id;e.value=edit.value;e.note_id=e.port_index=e.channel=e.key=-1;pushed=output->try_push(output,&e.header);}
        else{clap_event_param_gesture_t e{};e.header={sizeof(e),0,CLAP_CORE_EVENT_SPACE_ID,static_cast<std::uint16_t>(edit.gesture==cybridge::EditGesture::begin?CLAP_EVENT_PARAM_GESTURE_BEGIN:CLAP_EVENT_PARAM_GESTURE_END),CLAP_EVENT_IS_LIVE};e.param_id=edit.id;pushed=output->try_push(output,&e.header);}
        if(!pushed)break;read=static_cast<std::uint32_t>((read+1)%uiEdits.size());uiRead.store(read,std::memory_order_release);
    }
}
static bool CLAP_ABI guiSupported(const clap_plugin_t* p,const char* name,bool floating){auto& s=self(p);cybridge::WindowApi api{};auto* timer=s.hostTimer();try{return !floating&&timer&&timer->register_timer&&timer->unregister_timer&&windowApi(name,api)&&s.native->editorSupported(api);}catch(...){return false;}}
static bool CLAP_ABI guiPreferred(const clap_plugin_t* p,const char** api,bool* floating){
#ifdef _WIN32
    *api=CLAP_WINDOW_API_WIN32;
#elif defined(__APPLE__)
    *api=CLAP_WINDOW_API_COCOA;
#else
    *api=CLAP_WINDOW_API_X11;
#endif
    *floating=false;return guiSupported(p,*api,false);
}
static bool CLAP_ABI guiCreate(const clap_plugin_t* p,const char* name,bool floating){
    auto& s=self(p);if(s.editorCreated||!guiSupported(p,name,floating))return false;cybridge::WindowApi api{};windowApi(name,api);
    try{if(!s.native->createEditor(api,{&s,resized,edited,closed}))return false;
        if(!s.hostTimer()->register_timer(s.host,33,&s.editorTimer)){s.native->destroyEditor();return false;}s.editorCreated=true;return true;
    }catch(...){try{s.native->destroyEditor();}catch(...){}return false;}
}
static void CLAP_ABI guiDestroy(const clap_plugin_t* p){auto& s=self(p);if(!s.editorCreated)return;s.hostTimer()->unregister_timer(s.host,s.editorTimer);s.editorTimer=CLAP_INVALID_ID;try{s.native->destroyEditor();}catch(...){}s.editorCreated=false;}
static bool CLAP_ABI guiScale(const clap_plugin_t* p,double scale){try{return self(p).editorCreated&&self(p).native->setEditorScale(scale);}catch(...){return false;}}
static bool CLAP_ABI guiSize(const clap_plugin_t* p,std::uint32_t* w,std::uint32_t* h){try{auto& s=self(p);if(!s.editorCreated||!w||!h)return false;auto size=s.native->editorSize();*w=size.width;*h=size.height;return true;}catch(...){return false;}}
static bool CLAP_ABI guiCanResize(const clap_plugin_t* p){try{return self(p).editorCreated&&self(p).native->editorCanResize();}catch(...){return false;}}
static bool CLAP_ABI guiHints(const clap_plugin_t* p,clap_gui_resize_hints_t* hints){if(!hints||!guiCanResize(p))return false;*hints={true,true,false,0,0};return true;}
static bool CLAP_ABI guiAdjust(const clap_plugin_t* p,std::uint32_t* w,std::uint32_t* h){try{if(!w||!h)return false;cybridge::EditorSize size{*w,*h};if(!self(p).native->adjustEditorSize(size))return false;*w=size.width;*h=size.height;return true;}catch(...){return false;}}
static bool CLAP_ABI guiSetSize(const clap_plugin_t* p,std::uint32_t w,std::uint32_t h){try{return self(p).editorCreated&&self(p).native->resizeEditor({w,h});}catch(...){return false;}}
static bool CLAP_ABI guiParent(const clap_plugin_t* p,const clap_window_t* w){try{cybridge::WindowApi api{};return w&&windowApi(w->api,api)&&self(p).editorCreated&&self(p).native->setEditorParent({api,w->ptr});}catch(...){return false;}}
static bool CLAP_ABI guiTransient(const clap_plugin_t*,const clap_window_t*){return false;}
static void CLAP_ABI guiTitle(const clap_plugin_t*,const char*){}
static bool CLAP_ABI guiShow(const clap_plugin_t* p){try{return self(p).editorCreated&&self(p).native->showEditor(true);}catch(...){return false;}}
static bool CLAP_ABI guiHide(const clap_plugin_t* p){try{return self(p).editorCreated&&self(p).native->showEditor(false);}catch(...){return false;}}
static void CLAP_ABI timer(const clap_plugin_t* p,clap_id id){auto& s=self(p);if(s.editorCreated&&id==s.editorTimer)mainThread(p);}
