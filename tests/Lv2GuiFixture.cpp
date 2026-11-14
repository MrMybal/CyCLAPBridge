// SPDX-License-Identifier: MIT
#include <lv2/core/lv2.h>
#include <lv2/ui/ui.h>
#include <windows.h>
#include <cstring>
#include <cstdio>
#include <cstdint>
namespace {
struct Dsp{float* ports[5]{};};
LV2_Handle make(const LV2_Descriptor*,double,const char*,const LV2_Feature* const*){return new Dsp;}
void connect(LV2_Handle h,std::uint32_t port,void* data){if(port<5)static_cast<Dsp*>(h)->ports[port]=static_cast<float*>(data);}
void run(LV2_Handle h,std::uint32_t n){auto& s=*static_cast<Dsp*>(h);for(std::uint32_t i=0;i<n;++i){s.ports[2][i]=s.ports[0][i]*(*s.ports[4]);s.ports[3][i]=s.ports[1][i]*(*s.ports[4]);}}
void cleanup(LV2_Handle h){delete static_cast<Dsp*>(h);}
struct Ui{HWND window;LV2UI_Write_Function write;LV2UI_Controller controller;const LV2UI_Resize* resize=nullptr;const LV2UI_Touch* touch=nullptr;WNDPROC previous=nullptr;};
LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM w,LPARAM l){auto* ui=static_cast<Ui*>(GetPropW(window,L"CyBridgeLv2Fixture"));
    if(ui&&message==BM_CLICK){float value=.5f;if(ui->touch)ui->touch->touch(ui->touch->handle,4,true);ui->write(ui->controller,4,4,0,&value);if(ui->touch)ui->touch->touch(ui->touch->handle,4,false);
        if(ui->resize&&ui->resize->ui_resize(ui->resize->handle,480,240)==0)MoveWindow(window,0,0,480,240,TRUE);return 0;}
    return ui&&ui->previous?CallWindowProcW(ui->previous,window,message,w,l):DefWindowProcW(window,message,w,l);
}
LV2UI_Handle createUi(const LV2UI_Descriptor*,const char*,const char*,LV2UI_Write_Function write,LV2UI_Controller controller,LV2UI_Widget* widget,const LV2_Feature* const* features){
    HWND parent=nullptr;auto* ui=new Ui{};ui->write=write;ui->controller=controller;
    for(auto f=features;f&&*f;++f){auto* feature=*f;if(!std::strcmp(feature->URI,LV2_UI__parent))parent=static_cast<HWND>(feature->data);if(!std::strcmp(feature->URI,LV2_UI__resize))ui->resize=static_cast<const LV2UI_Resize*>(feature->data);if(!std::strcmp(feature->URI,LV2_UI__touch))ui->touch=static_cast<const LV2UI_Touch*>(feature->data);}
    if(!parent){delete ui;return nullptr;}
    ui->window=CreateWindowExW(0,L"BUTTON",L"LV2 gain",WS_CHILD|BS_PUSHBUTTON,0,0,320,180,parent,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!ui->window){delete ui;return nullptr;}SetPropW(ui->window,L"CyBridgeLv2Fixture",ui);
    ui->previous=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(ui->window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(procedure)));*widget=ui->window;return ui;
}
void destroyUi(LV2UI_Handle handle){auto* ui=static_cast<Ui*>(handle);DestroyWindow(ui->window);delete ui;}
void portEvent(LV2UI_Handle handle,std::uint32_t port,std::uint32_t size,std::uint32_t format,const void* data){if(port==4&&size==4&&!format){float value;std::memcpy(&value,data,4);wchar_t label[64];swprintf_s(label,L"LV2 gain %.3f",value);SetWindowTextW(static_cast<Ui*>(handle)->window,label);}}
int resizeUi(void* handle,int w,int h){MoveWindow(static_cast<Ui*>(handle)->window,0,0,w,h,TRUE);return 0;}
int idleUi(LV2UI_Handle){return 0;}
const void* extension(const char* uri){static const LV2UI_Resize resize{nullptr,resizeUi};static const LV2UI_Idle_Interface idle{idleUi};if(!std::strcmp(uri,LV2_UI__resize))return &resize;if(!std::strcmp(uri,LV2_UI__idleInterface))return &idle;return nullptr;}
}
extern "C" __declspec(dllexport) const LV2_Descriptor* lv2_descriptor(std::uint32_t index){static const LV2_Descriptor descriptor{"urn:cybridge:test:gui",make,connect,nullptr,run,nullptr,cleanup,nullptr};return index?nullptr:&descriptor;}
extern "C" __declspec(dllexport) const LV2UI_Descriptor* lv2ui_descriptor(std::uint32_t index){static const LV2UI_Descriptor descriptor{"urn:cybridge:test:gui#ui",createUi,destroyUi,portEvent,extension};return index?nullptr:&descriptor;}
