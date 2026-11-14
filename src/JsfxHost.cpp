// SPDX-License-Identifier: MIT
#include "HostedBackend.h"
#include "Process.h"
#include <ysfx.h>
#include <algorithm>
#include <array>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#include <windowsx.h>
#endif
namespace cybridge::detail {
namespace {
struct Jsfx final : HostedBackend {
    ysfx_t* fx=nullptr;
    std::array<const float*,64> in{};
    std::array<float*,64> out{};
#ifdef _WIN32
    HWND editor=nullptr;bool editorCreated=false,editorVisible=false;EditorSize dimensions{320,200};double scale=1;
    std::vector<std::uint32_t> pixels;
    static HINSTANCE editorModule(){HMODULE module=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&procedure),&module);return module;}
    static std::uint32_t modifiers(){return (GetKeyState(VK_SHIFT)<0?ysfx_mod_shift:0)|(GetKeyState(VK_CONTROL)<0?ysfx_mod_ctrl:0)|(GetKeyState(VK_MENU)<0?ysfx_mod_alt:0);}
    void configureGraphics(){
        pixels.resize(static_cast<std::size_t>(dimensions.width)*dimensions.height);
        ysfx_gfx_config_t config{};config.user_data=this;config.pixel_width=dimensions.width;config.pixel_height=dimensions.height;
        config.pixels=reinterpret_cast<std::uint8_t*>(pixels.data());config.scale_factor=scale;config.show_menu=menu;
        config.set_cursor=[](void*,std::int32_t cursor){SetCursor(LoadCursorW(nullptr,MAKEINTRESOURCEW(cursor==32649?32649:32512)));};
        ysfx_gfx_setup(fx,&config);
    }
    static int menu(void* handle,const char* text,std::int32_t x,std::int32_t y){
        auto& s=*static_cast<Jsfx*>(handle);auto* parsed=ysfx_parse_menu(text);if(!parsed)return 0;
        HMENU root=CreatePopupMenu();std::vector<HMENU> stack{root};
        for(std::uint32_t i=0;i<parsed->insn_count;++i){auto& instruction=parsed->insns[i];
            if(instruction.opcode==ysfx_menu_endsub){if(stack.size()>1)stack.pop_back();continue;}
            if(instruction.opcode==ysfx_menu_separator){AppendMenuW(stack.back(),MF_SEPARATOR,0,nullptr);continue;}
            auto name=std::filesystem::u8path(instruction.name?instruction.name:"").wstring();UINT flags=MF_STRING;
            if(instruction.item_flags&ysfx_menu_item_disabled)flags|=MF_GRAYED;if(instruction.item_flags&ysfx_menu_item_checked)flags|=MF_CHECKED;
            if(instruction.opcode==ysfx_menu_sub){auto child=CreatePopupMenu();AppendMenuW(stack.back(),flags|MF_POPUP,reinterpret_cast<UINT_PTR>(child),name.c_str());stack.push_back(child);}
            else AppendMenuW(stack.back(),flags,instruction.id,name.c_str());
        }
        POINT point{x,y};ClientToScreen(s.editor,&point);auto chosen=TrackPopupMenu(root,TPM_RETURNCMD|TPM_NONOTIFY,point.x,point.y,0,s.editor,nullptr);
        DestroyMenu(root);ysfx_menu_free(parsed);return chosen;
    }
    void paintTo(HDC dc){if(pixels.empty())return;BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=dimensions.width;info.bmiHeader.biHeight=-static_cast<LONG>(dimensions.height);info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        SetDIBitsToDevice(dc,0,0,dimensions.width,dimensions.height,0,0,0,dimensions.height,pixels.data(),&info,DIB_RGB_COLORS);}
    static LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM w,LPARAM l){
        auto* s=reinterpret_cast<Jsfx*>(GetWindowLongPtrW(window,GWLP_USERDATA));
        if(message==WM_NCCREATE){s=static_cast<Jsfx*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(s));}
        if(!s)return DefWindowProcW(window,message,w,l);
        switch(message){
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:{PAINTSTRUCT paint{};auto dc=BeginPaint(window,&paint);s->paintTo(dc);EndPaint(window,&paint);return 0;}
        case WM_PRINTCLIENT:s->paintTo(reinterpret_cast<HDC>(w));return 0;
        case WM_LBUTTONDOWN:case WM_RBUTTONDOWN:case WM_MBUTTONDOWN:SetFocus(window);SetCapture(window);[[fallthrough]];
        case WM_MOUSEMOVE:case WM_LBUTTONUP:case WM_RBUTTONUP:case WM_MBUTTONUP:{
            auto buttons=(w&MK_LBUTTON?ysfx_button_left:0)|(w&MK_RBUTTON?ysfx_button_right:0)|(w&MK_MBUTTON?ysfx_button_middle:0);
            ysfx_gfx_update_mouse(s->fx,modifiers(),GET_X_LPARAM(l),GET_Y_LPARAM(l),buttons,0,0);
            if(!buttons&&GetCapture()==window)ReleaseCapture();return 0;}
        case WM_MOUSEWHEEL:case WM_MOUSEHWHEEL:{POINT point{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(window,&point);auto delta=GET_WHEEL_DELTA_WPARAM(w)/120.0;
            ysfx_gfx_update_mouse(s->fx,modifiers(),point.x,point.y,0,message==WM_MOUSEWHEEL?delta:0,message==WM_MOUSEHWHEEL?delta:0);return 0;}
        case WM_CHAR:ysfx_gfx_add_key(s->fx,modifiers(),static_cast<std::uint32_t>(w),true);return 0;
        case WM_KEYDOWN:case WM_KEYUP:{std::uint32_t key=0;
            if(w>=VK_F1&&w<=VK_F12)key=ysfx_key_f1+static_cast<std::uint32_t>(w-VK_F1);
            else if(w==VK_LEFT)key=ysfx_key_left;else if(w==VK_RIGHT)key=ysfx_key_right;else if(w==VK_UP)key=ysfx_key_up;else if(w==VK_DOWN)key=ysfx_key_down;
            else if(w==VK_DELETE)key=ysfx_key_delete;else if(w==VK_HOME)key=ysfx_key_home;else if(w==VK_END)key=ysfx_key_end;
            else if(message==WM_KEYUP)key=MapVirtualKeyW(static_cast<UINT>(w),MAPVK_VK_TO_CHAR);
            if(key){ysfx_gfx_add_key(s->fx,modifiers(),key,message==WM_KEYDOWN);return 0;}break;}
        }return DefWindowProcW(window,message,w,l);
    }
    bool editorSupported(WindowApi a) const override{std::uint32_t size[2]{};return a==WindowApi::win32&&ysfx_get_gfx_dim(fx,size);}
    bool createEditor(WindowApi a,const EditorCallbacks& c) override{
        if(editorCreated||!editorSupported(a))return false;std::uint32_t size[2]{};ysfx_get_gfx_dim(fx,size);
        dimensions={size[0]?size[0]:320,size[1]?size[1]:200};if(!validSize(dimensions))return false;
        if(!prepared){ysfx_set_sample_rate(fx,48000);ysfx_set_block_size(fx,64);ysfx_init(fx);}
        editorCallbacks=c;editorCreated=true;configureGraphics();return true;
    }
    void destroyEditor() override{if(editor){DestroyWindow(editor);UnregisterClassW(L"CyBridgeYsfxGfx",editorModule());}editor=nullptr;editorCreated=editorVisible=false;editorCallbacks={};pixels.clear();}
    bool setEditorParent(NativeWindow parent) override{
        if(!editorCreated||editor||parent.api!=WindowApi::win32)return false;
        WNDCLASSW cls{};cls.lpfnWndProc=procedure;cls.hInstance=editorModule();cls.lpszClassName=L"CyBridgeYsfxGfx";cls.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));RegisterClassW(&cls);
        editor=CreateWindowExW(0,cls.lpszClassName,L"JSFX @gfx",WS_CHILD|WS_TABSTOP,0,0,dimensions.width,dimensions.height,static_cast<HWND>(parent.handle),nullptr,cls.hInstance,this);return editor;
    }
    EditorSize editorSize() const override{return dimensions;}
    bool editorCanResize() const override{return editorCreated;}
    bool adjustEditorSize(EditorSize& size) const override{return editorCreated&&validSize(size);}
    bool resizeEditor(EditorSize size) override{if(!editorCreated)return false;dimensions=size;configureGraphics();if(editor)MoveWindow(editor,0,0,size.width,size.height,TRUE);return true;}
    bool setEditorScale(double value) override{if(!editorCreated)return false;scale=value;configureGraphics();return true;}
    bool showEditor(bool visible) override{if(!editor)return false;editorVisible=visible;ShowWindow(editor,visible?SW_SHOWNA:SW_HIDE);return true;}
    std::uint32_t pump() override{
        control();if(editor&&editorVisible){
            if(ysfx_gfx_run(fx))InvalidateRect(editor,nullptr,FALSE);
            auto changes=ysfx_fetch_slider_changes(fx)|ysfx_fetch_slider_automations(fx);
            for(std::uint32_t i=0;i<64;++i)if(changes&(1ull<<i)){
                auto v=ysfx_slider_get_value(fx,i);edited(i,v,EditGesture::begin);edited(i,v,EditGesture::value);edited(i,v,EditGesture::end);
            }
        }return 0;
    }
#endif
    explicit Jsfx(const std::filesystem::path& path) {
        auto* config=ysfx_config_new();
        if(!config) throw std::bad_alloc();
        auto root=utf8(path.parent_path());
        ysfx_set_import_root(config,root.c_str()); ysfx_set_data_root(config,root.c_str());
        ysfx_register_builtin_audio_formats(config);
        fx=ysfx_new(config); ysfx_config_free(config);
        if(!fx) throw std::bad_alloc();
        try {
            if(!ysfx_load_file(fx,utf8(path).c_str(),0) || !ysfx_compile(fx,
#ifdef _WIN32
                0
#else
                ysfx_compile_no_gfx
#endif
            ))
                throw std::runtime_error("JSFX load/compilation failed (script/imports may require a newer EEL runtime)");
            info={"jsfx:"+utf8(path.filename()),ysfx_get_name(fx),ysfx_get_author(fx),"1"};
            auto inputs=ysfx_get_num_inputs(fx),outputs=ysfx_get_num_outputs(fx);
            if(inputs>64 || outputs>64) throw std::runtime_error("JSFX channel limit exceeded");
            if(inputs) ins.push_back({0,inputs,"JSFX input"});
            if(outputs) outs.push_back({0,outputs,"JSFX output"});
            for(std::uint32_t i=0;i<ysfx_max_sliders;++i) if(ysfx_slider_exists(fx,i)) {
                ysfx_slider_range_t range{}; ysfx_slider_get_range(fx,i,&range);
                params.push_back({i,ysfx_slider_get_name(fx,i),range.min,range.max,range.def,true,ysfx_slider_is_enum(fx,i)});
            }
        } catch(...) { ysfx_free(fx); fx=nullptr; throw; }
    }
    ~Jsfx() override {
#ifdef _WIN32
        destroyEditor();
#endif
        if(fx) ysfx_free(fx); }
    void prepare(double rate,std::uint32_t f,std::uint32_t e) override {
        limits(rate,f,e); ysfx_set_sample_rate(fx,rate); ysfx_set_block_size(fx,f);
        ysfx_set_midi_capacity(fx,std::max(4096u,e*32u),false); ysfx_init(fx);
        auto delay=ysfx_get_pdc_delay(fx);
        latency=std::isfinite(delay) && delay>0 ? static_cast<std::uint32_t>(std::min(delay,4294967295.0)) : 0;
        prepared=true;
    }
    void unprepare() override { idle(); prepared=false; }
    double currentParameter(std::uint32_t id) const override{return ysfx_slider_get_value(fx,id);}
    ProcessStatus process(ProcessBlock& b) noexcept override {
        if(!valid(b)) return running ? ProcessStatus::invalidBlock : ProcessStatus::notPrepared;
        std::size_t index=0;
        for(std::uint32_t begin=0;begin<b.frames;) {
            while(index<b.events.size() && b.events[index].frame==begin && b.events[index].kind==EventKind::parameter) {
                const auto& e=b.events[index++]; ysfx_slider_set_value(fx,e.parameterId,e.value);
            }
            std::uint32_t end=b.frames;
            for(std::size_t i=index;i<b.events.size();++i) if(b.events[i].kind==EventKind::parameter) { end=b.events[i].frame; break; }
            if(end==begin) {
                // All events at the boundary are handled before computing this slice.
                for(std::size_t i=index;i<b.events.size() && b.events[i].frame==begin;++i)
                    if(b.events[i].kind==EventKind::parameter) ysfx_slider_set_value(fx,b.events[i].parameterId,b.events[i].value);
                end=begin+1;
            }
            while(index<b.events.size() && b.events[index].frame<end) {
                const auto& e=b.events[index++];
                if(e.kind==EventKind::parameter) { ysfx_slider_set_value(fx,e.parameterId,e.value); continue; }
                std::uint8_t data[3]; std::memcpy(data,e.midi,3);
                if(e.kind!=EventKind::midi) {
                    data[0]=static_cast<std::uint8_t>((e.kind==EventKind::noteOn ? 0x90 : 0x80)|e.channel);
                    data[1]=static_cast<std::uint8_t>(e.key); data[2]=static_cast<std::uint8_t>(e.value*127);
                }
                if(e.port<0 || e.port>=16) return ProcessStatus::invalidBlock;
                ysfx_midi_event_t m{static_cast<std::uint32_t>(e.port),e.frame-begin,3,data};
                if(!ysfx_send_midi(fx,&m)) b.outputEventsDropped=true;
            }
            auto ni=ins.empty()?0u:ins[0].channels,no=outs.empty()?0u:outs[0].channels;
            for(std::uint32_t c=0;c<ni;++c) in[c]=b.inputs[0].channels[c]+begin;
            for(std::uint32_t c=0;c<no;++c) out[c]=b.outputs[0].channels[c]+begin;
            ysfx_process_float(fx,in.data(),out.data(),ni,no,end-begin);
            ysfx_midi_event_t m{};
            while(ysfx_receive_midi(fx,&m)) {
                if(m.size!=3 || m.offset>=end-begin) { b.outputEventsDropped=true; continue; }
                Event e; e.kind=EventKind::midi; e.frame=begin+m.offset; e.port=static_cast<std::int16_t>(m.bus);
                std::memcpy(e.midi,m.data,3); emit(b,e);
            }
            begin=end;
        }
        return ProcessStatus::ok;
    }
    std::vector<std::byte> save(std::size_t limit) override {
        idle(); auto* s=ysfx_save_state(fx);
        if(!s) throw std::runtime_error("JSFX state save failed");
        struct Guard { ysfx_state_t* s; ~Guard(){ysfx_state_free(s);} } guard{s};
        auto n=sizeof(std::uint32_t)+s->slider_count*(sizeof(std::uint32_t)+sizeof(double))+s->data_size;
        if(n>limit) throw std::runtime_error("JSFX state capacity exceeded");
        std::vector<std::byte> bytes(n); auto* out=bytes.data();
        auto put=[&](const void* p,std::size_t n){std::memcpy(out,p,n);out+=n;};
        put(&s->slider_count,4);
        for(std::uint32_t i=0;i<s->slider_count;++i){put(&s->sliders[i].index,4);put(&s->sliders[i].value,8);}
        if(s->data_size) put(s->data,s->data_size);
        return bytes;
    }
    void load(std::span<const std::byte> bytes) override {
        idle(); if(bytes.size()<4) throw std::runtime_error("Invalid JSFX state");
        std::uint32_t count; std::memcpy(&count,bytes.data(),4);
        if(count>64 || bytes.size()<4+count*12) throw std::runtime_error("Invalid JSFX state bounds");
        std::vector<ysfx_state_slider_t> sliders(count);
        for(std::uint32_t i=0;i<count;++i) {
            std::memcpy(&sliders[i].index,bytes.data()+4+i*12,4);
            std::memcpy(&sliders[i].value,bytes.data()+8+i*12,8);
            if(sliders[i].index>=64 || !std::isfinite(sliders[i].value)) throw std::runtime_error("Invalid JSFX slider state");
        }
        auto payload=bytes.subspan(4+count*12);
        ysfx_state_t state{sliders.data(),count,reinterpret_cast<std::uint8_t*>(const_cast<std::byte*>(payload.data())),payload.size()};
        if(!ysfx_load_state(fx,&state)) throw std::runtime_error("JSFX state restore failed");
    }
};
}
std::unique_ptr<HostedBackend> makeJsfx(const std::filesystem::path& path){return std::make_unique<Jsfx>(path);}
}
