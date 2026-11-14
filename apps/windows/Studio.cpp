// SPDX-License-Identifier: MIT
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <cyclapbridge/Conversion.h>
#include "AppPaths.h"
#include <algorithm>
#include <memory>
#include <string>
#include <thread>
#include <vector>
namespace {
constexpr UINT logMessage = WM_APP + 1, doneMessage = WM_APP + 2;
enum { inputId = 101, outputId, nameId, sourceBrowseId, outputBrowseId,
       generateId, buildId, openId, logId, backendId, backendBrowseId,
       sourceFormatId,targetFormatId,resourcesId,resourcesBrowseId,sourceFolderId,helpId };
std::wstring manualUrl() {
    auto path = cybridge::app::executablePath().parent_path().parent_path() /
        L"share/CyCLAPBridge/Documentation/Documentation.html";
    if(!std::filesystem::is_regular_file(path))
        path = std::filesystem::path(CYBRIDGE_SOURCE_ROOT) / L"Documentation/Documentation.html";
    if(!std::filesystem::is_regular_file(path)) return {};
    // The platform API encodes spaces, Unicode and reserved URL characters.
    std::wstring url(98304, L'\0');
    DWORD capacity = static_cast<DWORD>(url.size());
    if(FAILED(UrlCreateFromPathW(path.c_str(), url.data(), &capacity, 0))) return {};
    url.resize(wcslen(url.c_str()));
    return url + L"#/fr/studio/input";
}
std::wstring wide(const std::string& s) {
    if(s.empty()) return {};
    auto count = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring result(count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), result.data(), count);
    return result;
}
std::string narrow(const std::wstring& s) {
    auto u = std::filesystem::path(s).u8string();
    return {reinterpret_cast<const char*>(u.data()), u.size()};
}
std::wstring value(HWND window) {
    auto n = GetWindowTextLengthW(window);
    std::wstring result(static_cast<std::size_t>(n) + 1, L'\0');
    GetWindowTextW(window, result.data(), n + 1);
    result.resize(n); return result;
}
struct Studio {
    HWND window{}, title{}, subtitle{}, input{}, output{}, name{}, backend{}, log{}, status{}, generate{}, build{}, open{};
    HWND sourceFormat{},targetFormat{},resources{},resourceLabel{};
    cybridge::Format source() const {
        const cybridge::Format formats[]{cybridge::Format::clap,cybridge::Format::vst3,cybridge::Format::lv2,cybridge::Format::jsfx,cybridge::Format::auv2};
        auto index=SendMessageW(sourceFormat,CB_GETCURSEL,0,0);return formats[index>=0&&index<5?index:0];
    }
    cybridge::Format target() const {
        const cybridge::Format formats[]{cybridge::Format::vst3,cybridge::Format::clap,cybridge::Format::lv2,cybridge::Format::auv2,cybridge::Format::auv3};
        auto index=SendMessageW(targetFormat,CB_GETCURSEL,0,0);return formats[index>=0&&index<5?index:0];
    }
    HFONT font{}, heading{}, mono{};
    HBRUSH background = CreateSolidBrush(RGB(245,247,251)), white = CreateSolidBrush(RGB(255,255,255));
    std::vector<HWND> labels;
    std::thread worker;
    bool busy = false, smoke = false;
    std::filesystem::path artifacts;
    ~Studio() {
        if(worker.joinable()) worker.join();
        DeleteObject(font); DeleteObject(heading); DeleteObject(mono);
        DeleteObject(background); DeleteObject(white);
    }
    int px(int n) const { return MulDiv(n, static_cast<int>(GetDpiForWindow(window)), 96); }
    HWND control(const wchar_t* cls, const wchar_t* text, DWORD style, int id = 0, DWORD ex = 0) {
        auto h = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style,
            0,0,0,0,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
        SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE); return h;
    }
    void move(HWND h, int x, int y, int w, int height) { MoveWindow(h,px(x),px(y),px(w),px(height),TRUE); }
    void append(const std::wstring& text) {
        // Keep the UI log bounded, independent of compiler output volume.
        if(GetWindowTextLengthW(log) > 250000) SetWindowTextW(log, L"");
        SendMessageW(log, EM_SETSEL, static_cast<WPARAM>(-1), static_cast<LPARAM>(-1));
        SendMessageW(log, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
    }
    void layout() {
        RECT rect{}; GetClientRect(window, &rect);
        auto w = MulDiv(rect.right, 96, static_cast<int>(GetDpiForWindow(window)));
        auto h = MulDiv(rect.bottom, 96, static_cast<int>(GetDpiForWindow(window)));
        move(title,28,22,w-200,38); move(GetDlgItem(window,helpId),w-160,26,130,30);
        move(subtitle,30,66,w-60,42);
        move(sourceFormat,30,113,(w-80)/2,180);move(targetFormat,50+(w-80)/2,113,(w-80)/2,200);
        int rows[]{160,234,308,382};
        HWND fields[]{input,output,name,backend};
        for(int i=0;i<4;++i) {
            move(labels[i],30,rows[i],w-60,22);
            move(fields[i],30,rows[i]+25,w-(i==2 ? 60 : i==0?282:172),30);
        }
        move(GetDlgItem(window,sourceBrowseId),w-240,185,100,30);move(GetDlgItem(window,sourceFolderId),w-130,185,100,30);
        move(GetDlgItem(window,outputBrowseId),w-130,259,100,30);
        move(GetDlgItem(window,backendBrowseId),w-130,407,100,30);
        move(resourceLabel,30,458,w-60,22);move(resources,30,483,w-110,30);move(GetDlgItem(window,resourcesBrowseId),w-72,483,42,30);
        move(generate,30,538,170,36); move(build,214,538,190,36); move(open,w-200,538,170,36);
        move(status,30,590,w-60,38); move(log,30,634,w-60,std::max(50,h-661));
    }
    void create() {
        auto dpi = static_cast<int>(GetDpiForWindow(window));
        font = CreateFontW(-MulDiv(15,dpi,96),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        heading = CreateFontW(-MulDiv(29,dpi,96),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        mono = CreateFontW(-MulDiv(13,dpi,96),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Consolas");
        title = control(L"STATIC",L"CyCLAPBridge",0);
        SendMessageW(title,WM_SETFONT,reinterpret_cast<WPARAM>(heading),TRUE);
        control(L"BUTTON",L"Aide · F1",WS_TABSTOP,helpId);
        subtitle = control(L"STATIC",L"Wrappers CLAP, VST3, LV2 et AU · Sources JSFX prises en charge.\nChoisissez le format source à gauche et la cible à droite.",0);
        sourceFormat=control(L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,sourceFormatId);
        targetFormat=control(L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,targetFormatId);
        for(auto text:{L"Source : CLAP",L"Source : VST3",L"Source : LV2",L"Source : JSFX",L"Source : AUv2"})SendMessageW(sourceFormat,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text));
        for(auto text:{L"Cible : VST3",L"Cible : CLAP",L"Cible : LV2",L"Cible : AUv2 (macOS)",L"Cible : AUv3 (macOS/Xcode)"})SendMessageW(targetFormat,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text));
        SendMessageW(sourceFormat,CB_SETCURSEL,0,0);SendMessageW(targetFormat,CB_SETCURSEL,0,0);
        for(auto text : {L"Plugin source · Fichier ou bundle",L"Nouveau dossier du projet",L"Nom du wrapper",L"Dossier des composants CyCLAPBridge"})
            labels.push_back(control(L"STATIC",text,0));
        auto edit = [&](int id) { return control(L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL,id,WS_EX_CLIENTEDGE); };
        input=edit(inputId); output=edit(outputId); name=edit(nameId); backend=edit(backendId);
        resourceLabel=control(L"STATIC",L"Ressources/imports du plugin (facultatif)",0);
        resources=edit(resourcesId);
        control(L"BUTTON",L"…",WS_TABSTOP,resourcesBrowseId);
        SetWindowTextW(backend,cybridge::app::backendRoot().c_str());
        control(L"BUTTON",L"Fichier…",WS_TABSTOP,sourceBrowseId);control(L"BUTTON",L"Bundle…",WS_TABSTOP,sourceFolderId);
        control(L"BUTTON",L"Choisir…",WS_TABSTOP,outputBrowseId);
        control(L"BUTTON",L"Choisir…",WS_TABSTOP,backendBrowseId);
        generate=control(L"BUTTON",L"Créer le projet",WS_TABSTOP,generateId);
        build=control(L"BUTTON",L"Créer et compiler",WS_TABSTOP|BS_DEFPUSHBUTTON,buildId);
        open=control(L"BUTTON",L"Ouvrir le résultat",WS_TABSTOP,openId);
        EnableWindow(open,FALSE);
        status=control(L"STATIC",L"Prêt · CLAP → VST3",0);
        log=control(L"EDIT",L"",ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL,logId,WS_EX_CLIENTEDGE);
        SendMessageW(log,EM_SETLIMITTEXT,300000,0);
        SendMessageW(log,WM_SETFONT,reinterpret_cast<WPARAM>(mono),TRUE);
        append(L"Choisissez un plugin et un nouveau dossier, puis créez le projet ou compilez le wrapper.\r\n"
               L"Conservez le wrapper, ses ressources et ses notices ensemble.\r\n"
               L"AU exige macOS et une validation sur la plateforme Apple.\r\n"
               L"JSFX est un format source EEL2. L'hébergement audio est actuellement direct.\r\n");
        layout();
    }
    std::wstring folder(const wchar_t* caption) {
        IFileDialog* dialog = nullptr;
        if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))) return {};
        DWORD options{}; dialog->GetOptions(&options); dialog->SetOptions(options|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM);
        dialog->SetTitle(caption);
        std::wstring selected;
        if(SUCCEEDED(dialog->Show(window))) {
            IShellItem* item = nullptr;
            if(SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR path = nullptr;
                if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))) { selected=path; CoTaskMemFree(path); }
                item->Release();
            }
        }
        dialog->Release(); return selected;
    }
    void help() {
        auto url = manualUrl();
        if(url.empty()) { SetWindowTextW(status,L"Manuel introuvable · Conservez le dossier Documentation complet."); return; }
        if(reinterpret_cast<INT_PTR>(ShellExecuteW(window,L"open",url.c_str(),nullptr,nullptr,SW_SHOWNORMAL)) <= 32)
            SetWindowTextW(status,L"Impossible d'ouvrir le manuel dans le navigateur.");
    }
    void browseInput() {
        std::wstring buffer(32768,L'\0');
        OPENFILENAMEW dialog{}; dialog.lStructSize=sizeof(dialog); dialog.hwndOwner=window;
        dialog.lpstrFilter=L"Plugins et scripts\0*.clap;*.vst3;*.jsfx;*.txt\0Tous les fichiers\0*.*\0"; dialog.lpstrFile=buffer.data();
        dialog.nMaxFile=static_cast<DWORD>(buffer.size()); dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
        if(GetOpenFileNameW(&dialog)) {
            std::filesystem::path p(buffer.c_str());
            SetWindowTextW(input,p.c_str()); SetWindowTextW(name,p.stem().c_str());
            if(value(output).empty()) SetWindowTextW(output,(p.parent_path()/(p.stem().wstring()+L"-"+wide(cybridge::formatName(target()))+L"-project")).c_str());
        }
    }
    void convert(bool compile) {
        if(busy) return;
        cybridge::ConversionRequest request;
        request.input=value(input); request.projectDirectory=value(output);
        request.outputName=narrow(value(name)); request.backend=cybridge::backendFromRoot(value(backend));
        request.source=source();request.target=target();request.resources=value(resources);
        if(request.input.empty() || request.projectDirectory.empty()) {
            SetWindowTextW(status,L"Choisissez un plugin et un nouveau dossier de projet."); return;
        }
        busy=true; artifacts.clear(); EnableWindow(generate,FALSE); EnableWindow(build,FALSE); EnableWindow(open,FALSE);
        for(int id : {inputId,outputId,nameId,backendId,sourceBrowseId,outputBrowseId,backendBrowseId,sourceFolderId,sourceFormatId,targetFormatId,resourcesId,resourcesBrowseId})
            EnableWindow(GetDlgItem(window,id),FALSE);
        SetWindowTextW(status,compile ? L"Création et compilation en cours…" : L"Création du projet…");
        auto owner=window;
        worker=std::thread([owner,request=std::move(request),compile] {
            auto post=[owner](std::wstring text) {
                auto p=std::make_unique<std::wstring>(std::move(text));
                if(PostMessageW(owner,logMessage,0,reinterpret_cast<LPARAM>(p.get()))) p.release();
            };
            bool success=false;
            std::filesystem::path result;
            try {
                auto project=cybridge::createConversionProject(request);
                post(L"\r\nProjet créé : "+project.directory.wstring()+L"\r\n");
                result=project.directory;
                if(compile) {
                    auto buildResult=cybridge::buildConversionProject(project,cybridge::app::buildOptions(),
                        [&](const std::string& text) { post(wide(text)); });
                    if(buildResult.exitCode) throw std::runtime_error("La compilation a échoué. Consultez le journal.");
                    result=buildResult.artifactsDirectory;
                }
                post(L"\r\nTerminé : "+result.wstring()+L"\r\n"); success=true;
            } catch(const std::exception& e) { post(L"\r\nErreur : "+wide(e.what())+L"\r\n"); }
            auto p=std::make_unique<std::filesystem::path>(success ? result : std::filesystem::path{});
            if(PostMessageW(owner,doneMessage,success,reinterpret_cast<LPARAM>(p.get()))) p.release();
        });
    }
};
LRESULT CALLBACK procedure(HWND window,UINT msg,WPARAM w,LPARAM l) {
    auto* s=reinterpret_cast<Studio*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(msg==WM_NCCREATE) {
        s=static_cast<Studio*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        s->window=window; SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(s));
    }
    if(!s) return DefWindowProcW(window,msg,w,l);
    switch(msg) {
    case WM_CREATE: s->create(); return 0;
    case WM_SIZE: s->layout(); return 0;
    case WM_GETMINMAXINFO: {
        auto* info=reinterpret_cast<MINMAXINFO*>(l); info->ptMinTrackSize={s->px(740),s->px(800)}; return 0;
    }
    case WM_DPICHANGED: {
        auto* r=reinterpret_cast<RECT*>(l);
        SetWindowPos(window,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);
        s->layout(); return 0;
    }
    case WM_CTLCOLORSTATIC:
        SetTextColor(reinterpret_cast<HDC>(w),RGB(25,40,65));
        SetBkColor(reinterpret_cast<HDC>(w),RGB(245,247,251));
        if(reinterpret_cast<HWND>(l)==s->log) { SetBkColor(reinterpret_cast<HDC>(w),RGB(255,255,255)); return reinterpret_cast<LRESULT>(s->white); }
        return reinterpret_cast<LRESULT>(s->background);
    case WM_CTLCOLOREDIT:
        SetBkColor(reinterpret_cast<HDC>(w),RGB(255,255,255)); return reinterpret_cast<LRESULT>(s->white);
    case WM_COMMAND:
        switch(LOWORD(w)) {
        case helpId: s->help(); break;
        case sourceBrowseId: s->browseInput(); break;
        case sourceFolderId: {auto p=s->folder(L"Choisissez le bundle .lv2, .vst3 ou .component");if(!p.empty()){SetWindowTextW(s->input,p.c_str());SetWindowTextW(s->name,std::filesystem::path(p).stem().c_str());}break;}
        case resourcesBrowseId:{auto p=s->folder(L"Dossier contenant le plugin et ses ressources/imports");if(!p.empty())SetWindowTextW(s->resources,p.c_str());break;}
        case outputBrowseId: {
            auto p=s->folder(L"Choisissez le dossier parent du nouveau projet");
            if(!p.empty()) {
                auto n=value(s->name); if(n.empty()) n=L"Plugin";
                SetWindowTextW(s->output,(std::filesystem::path(p)/(n+L"-"+wide(cybridge::formatName(s->target()))+L"-project")).c_str());
            }
            break;
        }
        case backendBrowseId: { auto p=s->folder(L"Dossier contenant third_party et templates"); if(!p.empty()) SetWindowTextW(s->backend,p.c_str()); break; }
        case generateId: s->convert(false); break;
        case buildId: s->convert(true); break;
        case openId: if(!s->artifacts.empty()) ShellExecuteW(window,L"open",s->artifacts.c_str(),nullptr,nullptr,SW_SHOWNORMAL); break;
        }
        return 0;
    case WM_HELP: s->help(); return TRUE;
    case logMessage: { std::unique_ptr<std::wstring> text(reinterpret_cast<std::wstring*>(l)); s->append(*text); return 0; }
    case doneMessage: {
        std::unique_ptr<std::filesystem::path> path(reinterpret_cast<std::filesystem::path*>(l));
        if(s->worker.joinable()) s->worker.join();
        s->busy=false; s->artifacts=*path;
        EnableWindow(s->generate,TRUE); EnableWindow(s->build,TRUE); EnableWindow(s->open,w ? TRUE : FALSE);
        for(int id : {inputId,outputId,nameId,backendId,sourceBrowseId,outputBrowseId,backendBrowseId,sourceFolderId,sourceFormatId,targetFormatId,resourcesId,resourcesBrowseId})
            EnableWindow(GetDlgItem(window,id),TRUE);
        SetWindowTextW(s->status,w ? L"Terminé · Le résultat est prêt." : L"Échec · Consultez le journal pour corriger le problème.");
        return 0;
    }
    case WM_CLOSE:
        if(s->busy) { SetWindowTextW(s->status,L"Compilation en cours · Attendez sa fin avant de fermer."); return 0; }
        DestroyWindow(window); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window,msg,w,l);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR command,int show) {
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_STANDARD_CLASSES}; InitCommonControlsEx(&controls);
    Studio studio;
    WNDCLASSW cls{}; cls.hInstance=instance; cls.lpfnWndProc=procedure; cls.lpszClassName=L"CyCLAPBridgeStudio";
    cls.hCursor=LoadCursorW(nullptr,IDC_ARROW); cls.hIcon=LoadIconW(nullptr,IDI_APPLICATION); cls.hbrBackground=studio.background;
    RegisterClassW(&cls);
    auto window=CreateWindowExW(0,cls.lpszClassName,L"CyCLAPBridge Studio",WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,CW_USEDEFAULT,960,900,nullptr,nullptr,instance,&studio);
    if(!window) { CoUninitialize(); return 1; }
    if(std::wstring(command)==L"--smoke-test") {
        auto ok=IsWindow(studio.input)&&IsWindow(studio.output)&&IsWindow(studio.log)&&IsWindow(studio.build)
            &&SendMessageW(studio.sourceFormat,CB_GETCOUNT,0,0)==5
            &&SendMessageW(studio.targetFormat,CB_GETCOUNT,0,0)==5
            &&IsWindow(studio.resources)&&IsWindow(GetDlgItem(window,helpId))
            &&manualUrl().find(L"#/fr/studio/input")!=std::wstring::npos;
        DestroyWindow(window); CoUninitialize(); return ok ? 0 : 2;
    }
    ShowWindow(window,show); UpdateWindow(window);
    MSG message{};
    while(GetMessageW(&message,nullptr,0,0)>0) {
        if(message.message==WM_KEYDOWN&&message.wParam==VK_F1) { studio.help(); continue; }
        if(!IsDialogMessageW(window,&message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    CoUninitialize(); return static_cast<int>(message.wParam);
}
