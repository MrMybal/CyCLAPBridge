// SPDX-License-Identifier: MIT
#include <cyclapbridge/Plugin.h>
#include <windows.h>
#include <array>
#include <chrono>
#include <thread>
#include <iostream>
#include <cmath>
#include <atomic>
#include <cstdlib>
#include <new>
thread_local bool auditing=false;
thread_local unsigned allocations=0,frees=0;
void* operator new(std::size_t n){if(auditing)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept{if(auditing&&p)++frees;std::free(p);}
void operator delete[](void* p) noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept{::operator delete(p);}
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Parent{HWND window=CreateWindowExW(0,L"STATIC",L"CyBridge hidden GUI acceptance",WS_OVERLAPPEDWINDOW,0,0,800,600,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);~Parent(){if(window)DestroyWindow(window);}};
struct Callbacks{HWND parent;unsigned edits=0,resizes=0;
    static bool resized(void* p,cybridge::EditorSize size) noexcept{auto& s=*static_cast<Callbacks*>(p);++s.resizes;return MoveWindow(s.parent,0,0,size.width,size.height,FALSE)!=FALSE;}
    static void edited(void* p,std::uint32_t,double,cybridge::EditGesture gesture) noexcept{if(gesture==cybridge::EditGesture::value)++static_cast<Callbacks*>(p)->edits;}
};
HWND findChild(HWND parent,const wchar_t* cls){struct Search{const wchar_t* cls;HWND found=nullptr;} search{cls};EnumChildWindows(parent,[](HWND child,LPARAM data)->BOOL{auto& s=*reinterpret_cast<Search*>(data);wchar_t name[128];GetClassNameW(child,name,128);if(!wcscmp(name,s.cls)){s.found=child;return FALSE;}return TRUE;},reinterpret_cast<LPARAM>(&search));return search.found;}
void pump(cybridge::HostedPlugin& plugin){std::this_thread::sleep_for(std::chrono::milliseconds(40));plugin.pumpMainThread();MSG m;while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}}
int wmain(int argc,wchar_t** argv){try{
    check(argc==3,"Expected format and fixture path");auto u=std::filesystem::path(argv[1]).u8string();auto format=cybridge::parseFormat({reinterpret_cast<const char*>(u.data()),u.size()});
    Parent parent;check(parent.window,"Cannot create hidden parent");cybridge::HostedPlugin plugin(format,std::filesystem::absolute(argv[2]));
    check(plugin.editorSupported(cybridge::WindowApi::win32),"Native editor is missing");check(!plugin.editorSupported(cybridge::WindowApi::cocoa),"Wrong platform editor advertised");
    std::array<float,64> a,b,c,d;a.fill(.25f);b.fill(-.5f);float* in[]{a.data(),b.data()},*out[]{c.data(),d.data()};
    std::array<cybridge::AudioBus,1> inputs{{{in,2}}},outputs{{{out,2}}};std::array<cybridge::Event,128> received{};
    cybridge::ProcessBlock block{64,inputs,outputs,{},received};
    for(int round=0;round<3;++round){
        Callbacks callbacks{parent.window};check(plugin.createEditor(cybridge::WindowApi::win32,{&callbacks,Callbacks::resized,Callbacks::edited,nullptr}),"Editor creation failed");
        check(!plugin.createEditor(cybridge::WindowApi::win32),"Duplicate editor accepted");
        auto size=plugin.editorSize();check(size.width&&size.height,"Missing initial size");
        check(!plugin.resizeEditor({0,0}),"Invalid dimensions accepted");
        check(plugin.setEditorParent({cybridge::WindowApi::win32,parent.window}),"Editor attachment failed");
        check(plugin.showEditor(true),"Editor show failed");
        // AU/DAW lifecycle permits opening an editor before activation.
        plugin.prepare(48000,64);check(plugin.start(),"Audio start failed");pump(plugin);
        auto button=findChild(parent.window,L"Button");auto gfx=findChild(parent.window,L"CyBridgeYsfxGfx");check(button||gfx,"No native child widget");
        if(button)SendMessageW(button,BM_CLICK,0,0);
        else{SendMessageW(gfx,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(12,12));pump(plugin);SendMessageW(gfx,WM_LBUTTONUP,0,MAKELPARAM(12,12));}
        bool notified=false;
        // Real DAWs keep rendering while wrappers deliver asynchronous GUI edits.
        for(int settle=0;settle<4;++settle){pump(plugin);check(plugin.process(block)==cybridge::ProcessStatus::ok,"Processing with GUI failed");pump(plugin);notified=notified||callbacks.edits||block.outputEventCount;if(notified)break;}
        if(gfx){auto dc=GetDC(gfx);auto memory=CreateCompatibleDC(dc);auto bitmap=CreateCompatibleBitmap(dc,32,32);auto previous=SelectObject(memory,bitmap);
            SendMessageW(gfx,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(memory),PRF_CLIENT);auto pixel=GetPixel(memory,10,10);
            SelectObject(memory,previous);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(gfx,dc);
            check(pixel!=CLR_INVALID&&std::abs(static_cast<int>(GetRValue(pixel))-51)<=1&&std::abs(static_cast<int>(GetGValue(pixel))-102)<=1&&std::abs(static_cast<int>(GetBValue(pixel))-153)<=1,"JSFX @gfx pixels are incorrect");}
        check(std::abs(c[63]-.125f)<1e-6&&std::abs(d[63]+.25f)<1e-6,"GUI edit did not reach audio");
        check(notified,"GUI automation was not delivered");
        if(button)check(callbacks.resizes>0,"Plugin resize request was not delivered");
        if(plugin.editorCanResize()){cybridge::EditorSize resized{400,220};check(plugin.adjustEditorSize(resized),"Resize constraints failed");check(plugin.resizeEditor(resized),"Editor resize failed");}
        auto parameter=plugin.parameters().front();cybridge::Event e;e.parameterId=parameter.id;e.value=parameter.maximum==1?.75:1.5;block.events={&e,1};
        check(plugin.process(block)==cybridge::ProcessStatus::ok,"Host automation failed");pump(plugin);check(std::abs(c[63]-.375f)<1e-6,"Host automation audio mismatch");block.events={};
        plugin.stop();std::atomic<bool> rendering{true},renderOk{true};
        std::thread audio([&]{if(!plugin.start()){renderOk=false;rendering=false;return;}for(int i=0;i<32;++i){
            allocations=frees=0;auditing=true;auto status=plugin.process(block);auditing=false;
            if(status!=cybridge::ProcessStatus::ok||allocations||frees)renderOk=false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));}plugin.stop();rendering=false;});
        while(rendering.load()){plugin.pumpMainThread();double value=0;if(!plugin.parameterValue(parameter.id,value)||!std::isfinite(value))renderOk=false;std::this_thread::sleep_for(std::chrono::milliseconds(1));}audio.join();check(renderOk,"Concurrent GUI/audio or prepared allocation audit failed");
        bool refused=false;std::thread wrong([&]{try{plugin.destroyEditor();}catch(const std::logic_error&){refused=true;}});wrong.join();check(refused,"Wrong-thread editor destruction accepted");
        check(plugin.showEditor(false),"Editor hide failed");check(plugin.showEditor(true),"Editor reshow failed");
        plugin.stop();
        // A stopped editor edit must survive a save before another audio block.
        auto expected=parameter.maximum==1?.25:.5;
        if(button)SendMessageW(button,BM_CLICK,0,0);else SendMessageW(gfx,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(12,12));
        bool acknowledged=false;
        for(int frame=0;frame<12;++frame){pump(plugin);double value=0;if(plugin.parameterValue(parameter.id,value)&&std::abs(value-expected)<1e-6){acknowledged=true;break;}}
        if(gfx)SendMessageW(gfx,WM_LBUTTONUP,0,MAKELPARAM(12,12));
        check(acknowledged,"Stopped GUI input was not acknowledged");pump(plugin);
        double before=0;plugin.parameterValue(parameter.id,before);auto state=plugin.saveState();plugin.loadState(state);plugin.release();plugin.destroyEditor();
        {cybridge::HostedPlugin restored(format,std::filesystem::absolute(argv[2]));restored.loadState(state);restored.prepare(48000,64);check(restored.start(),"Restored start failed");auto status=restored.process(block);if(status!=cybridge::ProcessStatus::ok||std::abs(c[63]-.125f)>=1e-6)std::cerr<<"round "<<round<<", before="<<before<<", restored sample="<<c[63]<<'\n';check(status==cybridge::ProcessStatus::ok&&std::abs(c[63]-.125f)<1e-6,"Stopped GUI edit was not preserved in state");restored.stop();restored.release();}
        check(!findChild(parent.window,L"Button")&&!findChild(parent.window,L"CyBridgeYsfxGfx"),"Native widget leaked after closing");
    }
    std::cout<<"PASS: native editor lifecycle, input, audio/automation, resizing, thread ownership, allocation audit, stopped-edit state and cleanup\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
