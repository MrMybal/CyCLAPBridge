// SPDX-License-Identifier: MIT
#include "AppPaths.h"
#include "StudioChat.h"
#include "StudioTools.h"
#include "StudioPlatform.h"
#include <cyui/Core.h>
#include <cyui/modules/FilePicker.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_stdlib.h>
#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif
namespace {
namespace fs=std::filesystem;
using namespace cybridge;
using namespace cybridge::studio;
constexpr std::array sources{Format::clap,Format::vst3,Format::lv2,Format::jsfx,Format::auv2};
constexpr std::array targets{Format::vst3,Format::clap,Format::lv2,Format::auv2,Format::auv3};
void translations() {
    cyui::registerTranslations("fr",{
        {"Plugin wrappers and format conversion","Wrappers de plugins et conversion de formats"},
        {"Language","Langue"},{"Theme","Thème"},{"Conversion","Conversion"},
        {"Source format","Format source"},{"Target format","Format cible"},
        {"Input plugin","Plugin d’entrée"},{"File","Fichier"},{"Bundle","Bundle"},
        {"Output folder","Dossier de destination"},{"Output name","Nom du résultat"},
        {"Resources and imports","Ressources et imports"},{"Browse","Parcourir"},
        {"Advanced options","Options avancées"},{"Component folder","Dossier des composants"},
        {"Plugin ID (optional)","ID du plugin (facultatif)"},{"CMake executable","Exécutable CMake"},
        {"Generator (optional)","Générateur (facultatif)"},{"Architecture (optional)","Architecture (facultative)"},
        {"Configuration","Configuration"},{"Create project","Créer le projet"},
        {"Convert","Convertir"},{"Open result","Ouvrir le résultat"},
        {"Conversion log","Journal de conversion"},{"Copy log","Copier le journal"},
        {"Ready","Prêt"},{"Preparing conversion…","Préparation de la conversion…"},
        {"Building…","Compilation…"},{"Completed","Terminé"},{"Failed","Échec"},
        {"No build output yet.","Aucune sortie de compilation pour le moment."},
        {"Source and target must be different.","La source et la cible doivent être différentes."},
        {"Keep the wrapper, payload, resources and notices together.","Conservez ensemble le wrapper, le payload, les ressources et les notices."},
        {"Apple conversion requires macOS; AUv3 also requires Xcode. Apple validation is deferred.","La conversion Apple demande macOS ; AUv3 demande aussi Xcode. La validation Apple est reportée."},
        {"Choose a file or folder","Choisir un fichier ou dossier"},{"Location","Emplacement"},
        {"Help (F1)","Aide (F1)"},{"Cannot open the local manual.","Impossible d’ouvrir le manuel local."},
        {"Cannot open the result folder.","Impossible d’ouvrir le dossier du résultat."},
        {"Build in progress. Wait before closing.","Compilation en cours. Attendez avant de fermer."},
        {"Chat","Tchat"},{"Providers","Fournisseurs"},{"Provider","Fournisseur"},
        {"Demo / Markdown (no AI)","Démo Markdown (sans IA)"},
        {"Detect providers","Détecter les fournisseurs"},
        {"No ready provider detected.","Aucun fournisseur prêt détecté."},
        {"Provider detection failed.","La détection des fournisseurs a échoué."},
        {"Model (optional)","Modèle (facultatif)"},{"Available models","Modèles disponibles"},
        {"Endpoint","Adresse du service"},{"API key environment variable","Variable d’environnement de la clé API"},
        {"Invalid API key environment variable name.","Nom de variable d’environnement invalide."},
        {"Set the API key environment variable and restart Studio.","Définissez la variable d’environnement de la clé API puis relancez Studio."},
        {"Executable (optional)","Exécutable (facultatif)"},
        {"Give this chat access to Studio tools","Donner à ce tchat accès aux outils de Studio"},
        {"Connect / New chat","Connecter / Nouveau tchat"},
        {"Save chat history locally","Enregistrer l’historique sur cet ordinateur"},
        {"History ID","ID de l’historique"},{"Restore saved chat","Restaurer le tchat enregistré"},
        {"Studio cannot locate the local history folder.","Studio ne trouve pas le dossier d’historique local."},
        {"External assistants (MCP)","Assistants externes (MCP)"},
        {"MCP access profile","Profil d’accès MCP"},
        {"Free","Free"},{"Local controlled","Local contrôlé"},
        {"Token only","Jeton seul"},{"Protected","Protégé"},
        {"Bearer token required.","Jeton Bearer requis."},
        {"Local clients can connect with the address alone.","Les clients locaux peuvent se connecter avec l’adresse seule."},
        {"Studio asks before starting inspections or conversions.","Studio demande une validation avant les inspections et les conversions."},
        {"Inspections and conversions start without a confirmation.","Les inspections et conversions démarrent sans confirmation."},
        {"Start local MCP server","Démarrer le serveur MCP local"},
        {"Stop local MCP server","Arrêter le serveur MCP local"},
        {"Copy MCP connection","Copier la connexion MCP"},
        {"Assistant request","Demande d’un assistant"},
        {"External MCP assistant","Assistant MCP externe"},{"Studio chat","Tchat Studio"},
        {"Inspect plugin","Inspecter le plugin"},{"Convert plugin","Convertir le plugin"},
        {"Native plugin code may run during this operation.","Le code natif du plugin peut s’exécuter pendant cette opération."},
        {"Run request","Exécuter la demande"},{"Decline","Refuser"},
        {"Mid grey","Gris moyen"},{"Light grey","Gris clair"}
    });
}
std::string label(const char* text){return cyui::trLabel(text);}
void text(const char* value){ImGui::TextWrapped("%s",cyui::tr(value).c_str());}

struct Picker {
    enum class Field { input, output, resources, backend, attachment } field{};
    bool open=false;
    cyui::FilePickerState state;
    cyui::FilePickerOptions options;
    cyui::FileListing listing;
    std::future<cyui::FileListing> work;
    std::string pending;
    std::vector<std::string> volumes=roots();
    void begin(Field destination,const std::string& value,bool directory) {
        field=destination;open=true;state={};listing={};
        auto path=value.empty()?fs::current_path():fs::absolute(pathFromText(value));
        if(!value.empty()&&(destination==Field::input||destination==Field::output))path=path.parent_path();
        if(path.empty())path=fs::current_path();
        state.directory=pathText(path);state.pathText=state.directory;
        options={};options.mode=directory?cyui::FilePickerMode::directory:cyui::FilePickerMode::openFile;
        options.root=pathText(path.root_path());
        if(options.root.empty())options.root=volumes.empty()?"/":volumes.front();
        if(!directory&&destination!=Field::attachment)options.filters={{"CLAP / VST3 / JSFX",{"clap","vst3","jsfx","txt"}},{"All files",{}}};
    }
    void update() {
        using namespace std::chrono_literals;
        if(work.valid()&&work.wait_for(0s)==std::future_status::ready) {
            auto result=work.get();if(result.directory==state.directory)listing=std::move(result);
        }
        if(!work.valid()&&listing.directory!=state.directory) {
            pending=state.directory;listing={};listing.directory=pending;listing.loading=true;
            work=std::async(std::launch::async,[directory=pending]{
                cyui::FileListing result;result.directory=directory;
                try {
                    std::uint64_t id=1;
                    for(const auto& entry:fs::directory_iterator(pathFromText(directory),fs::directory_options::skip_permission_denied)) {
                        std::error_code error;const bool folder=entry.is_directory(error);
                        auto bytes=folder?0:entry.file_size(error);
                        result.entries.push_back({id++,pathText(entry.path().filename()),folder,error?0:bytes,{}});
                    }
                    std::sort(result.entries.begin(),result.entries.end(),[](const auto& a,const auto& b){return a.directory!=b.directory?a.directory>b.directory:a.name<b.name;});
                }catch(const std::exception& e){result.error=e.what();}
                return result;
            });
        }
    }
    std::string draw() {
        if(!open)return {};
        std::string accepted;
        if(cyui::beginModal(label("Choose a file or folder").c_str(),&open,820)) {
            if(volumes.size()>1&&ImGui::BeginCombo(label("Location").c_str(),options.root.c_str())) {
                for(const auto& volume:volumes)if(ImGui::Selectable(volume.c_str(),volume==options.root)){
                    options.root=volume;state={};state.directory=volume;state.pathText=volume;
                }
                ImGui::EndCombo();
            }
            update();
            auto result=cyui::filePicker("##filesystem",state,listing,cyui::dp(0,460),options);
            if(result.accepted&&!result.paths.empty()){accepted=result.paths.front();open=false;}
            if(result.cancelled)open=false;
            cyui::endModal();
        }
        return accepted;
    }
};
struct Studio {
    Model model;
    Picker picker;
    StudioTools tools{model};
    StudioChat chat{tools,[this]{picker.begin(Picker::Field::attachment,{},false);}};
    bool chatVisible=false;
    int source=0,target=0;
    std::string input,output,name,resources,pluginId;
    std::string backend=pathText(cybridge::app::backendRoot());
    BuildOptions build=cybridge::app::buildOptions();
    std::string cmake=pathText(build.cmake);
    void help() {
        auto path=cybridge::app::dataRoot()/"Documentation/Documentation.html";
        auto url=fs::is_regular_file(path)?fileUrl(path):std::string{};
        if(url.empty()||!openUrl(url+"#/"+cyui::locale()+(chatVisible?"/studio/assistants":"/studio/input")))
            cyui::notify(cyui::tr("Cannot open the local manual.").c_str(),cyui::Tone::error);
    }
    void picked(const std::string& path) {
        if(path.empty())return;
        switch(picker.field) {
        case Picker::Field::input:
            input=path;name=pathText(pathFromText(path).stem());
            if(output.empty())output=pathText(pathFromText(path).parent_path()/(pathFromText(name+"-"+formatName(targets[target])+"-converted")));
            break;
        case Picker::Field::output:output=pathText(pathFromText(path)/pathFromText((name.empty()?"Plugin":name)+"-"+formatName(targets[target])+"-converted"));break;
        case Picker::Field::resources:resources=path;break;
        case Picker::Field::backend:backend=path;break;
        case Picker::Field::attachment:chat.attach(pathFromText(path));break;
        }
    }
    void pathField(const char* caption,std::string& value,Picker::Field field,bool bundle=false) {
        cyui::property(cyui::tr(caption).c_str());
        const float reserve=cyui::dp(bundle?152.f:88.f);
        ImGui::SetNextItemWidth(std::max(cyui::dp(80.f),ImGui::GetContentRegionAvail().x-reserve));
        ImGui::InputText((std::string("##")+caption).c_str(),&value);
        ImGui::SameLine();ImGui::PushID(caption);
        if(cyui::button(label(bundle?"File":"Browse").c_str()))picker.begin(field,value,!bundle);
        if(bundle){ImGui::SameLine();if(cyui::button(label("Bundle").c_str()))picker.begin(field,value,true);}
        ImGui::PopID();
    }
    fs::path workRoot() const {
        const auto settings=settingsFile();
        return (settings.empty()?fs::temp_directory_path()/"CyCLAPBridge":settings.parent_path())/"Work";
    }
    void approveRequest(const fs::path& work) {
        auto options=build;
        options.cmake=pathFromText(cmake);
        tools.approve(backendFromRoot(pathFromText(backend)),std::move(options),work);
    }
    void startQueuedRequest(const fs::path& workOverride) {
        if(!tools.queued()||model.busy())return;
        try {
            auto options=build;
            options.cmake=pathFromText(cmake);
            tools.startQueued(backendFromRoot(pathFromText(backend)),std::move(options),
                              workOverride.empty()?workRoot():workOverride);
        }catch(const std::exception& e){tools.failQueued(e.what());}
    }
    void drawRequest() {
        const auto pending=tools.pending();
        if(!pending)return;
        cyui::sectionTitle(cyui::tr("Assistant request").c_str());
        text(pending->external?"External MCP assistant":"Studio chat");
        text(pending->operation=="inspect_plugin"?"Inspect plugin":"Convert plugin");
        const auto fields=pending->arguments.dump(2);
        if(ImGui::BeginChild("##request-paths",cyui::dp(0,140),ImGuiChildFlags_Borders))
            ImGui::TextWrapped("%s",fields.c_str());
        ImGui::EndChild();
        text("Native plugin code may run during this operation.");
        ImGui::BeginDisabled(model.busy());
        if(cyui::primaryButton(label("Run request").c_str())) {
            try {
                approveRequest(workRoot());
            }catch(const std::exception& e){cyui::notify(e.what(),cyui::Tone::error);}
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if(cyui::button(label("Decline").c_str()))tools.decline();
        ImGui::Separator();
    }
    void start() {
        try {
            ConversionRequest request;request.source=sources[source];request.target=targets[target];
            request.input=pathFromText(input);request.projectDirectory=pathFromText(output);request.outputName=name;
            request.resources=pathFromText(resources);request.pluginId=pluginId;request.backend=backendFromRoot(pathFromText(backend));
            build.cmake=pathFromText(cmake);
#ifdef __APPLE__
            if(request.target==Format::auv3&&build.generator.empty())build.generator="Xcode";
#endif
            model.convert(std::move(request),build,workRoot());
        }catch(const std::exception& e){cyui::notify(e.what(),cyui::Tone::error);}
    }
    void draw(const fs::path& workOverride = {}) {
        tools.poll();
        startQueuedRequest(workOverride);
        chat.poll();
        auto* viewport=ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("CyCLAPBridge Studio",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
        cyui::heading("CyCLAPBridge Studio",1);
        cyui::textDim("%s",cyui::tr("Plugin wrappers and format conversion").c_str());
        cyui::badge("0.2.2",cyui::Tone::neutral);ImGui::SameLine();
        ImGui::SetNextItemWidth(cyui::dp(150));
        if(ImGui::BeginCombo(label("Language").c_str(),cyui::locale()=="fr"?"Français":"English")) {
            for(auto entry:std::array<std::pair<const char*,const char*>,2>{{{"en","English"},{"fr","Français"}}})
                if(ImGui::Selectable(entry.second,cyui::locale()==entry.first))cyui::setLocale(entry.first);
            ImGui::EndCombo();
        }
        ImGui::SameLine();ImGui::SetNextItemWidth(cyui::dp(165));
        if(ImGui::BeginCombo(label("Theme").c_str(),cyui::currentThemeKey().c_str())) {
            for(const auto& theme:cyui::builtInThemes())if(ImGui::Selectable(cyui::tr(theme.name.c_str()).c_str(),theme.key==cyui::currentThemeKey()))cyui::setTheme(theme.key.c_str());
            ImGui::EndCombo();
        }
        ImGui::SameLine();if(cyui::button(label("Help (F1)").c_str())||ImGui::IsKeyPressed(ImGuiKey_F1))help();
        ImGui::SameLine();
        if(cyui::button(label(chatVisible?"Conversion":"Chat").c_str()))chatVisible=!chatVisible;
        cyui::space(10);
        drawRequest();
        if(chatVisible)chat.draw();else drawConversion();
        picked(picker.draw());
        ImGui::End();cyui::renderNotifications();
    }
    void drawConversion() {
        if(cyui::beginCard("##conversion",cyui::tr("Conversion").c_str())) {
            ImGui::BeginDisabled(model.busy());
            if(cyui::beginProperties("##fields",180)) {
                cyui::property(cyui::tr("Source format").c_str());
                if(ImGui::BeginCombo("##source",formatName(sources[source]).c_str())){for(int i=0;i<static_cast<int>(sources.size());++i)if(ImGui::Selectable(formatName(sources[i]).c_str(),i==source))source=i;ImGui::EndCombo();}
                cyui::property(cyui::tr("Target format").c_str());
                if(ImGui::BeginCombo("##target",formatName(targets[target]).c_str())){for(int i=0;i<static_cast<int>(targets.size());++i)if(ImGui::Selectable(formatName(targets[i]).c_str(),i==target))target=i;ImGui::EndCombo();}
                pathField("Input plugin",input,Picker::Field::input,true);
                pathField("Output folder",output,Picker::Field::output);
                cyui::property(cyui::tr("Output name").c_str());ImGui::InputText("##name",&name);
                pathField("Resources and imports",resources,Picker::Field::resources);
                cyui::endProperties();
            }
            if(cyui::beginSection(label("Advanced options").c_str(),false)) {
                if(cyui::beginProperties("##advanced",180)) {
                    for(auto field:std::array<std::pair<const char*,std::string*>,5>{{{"Plugin ID (optional)",&pluginId},{"CMake executable",&cmake},{"Generator (optional)",&build.generator},{"Architecture (optional)",&build.architecture},{"Configuration",&build.configuration}}}){
                        cyui::property(cyui::tr(field.first).c_str());ImGui::InputText((std::string("##")+field.first).c_str(),field.second);
                    }
                    cyui::endProperties();
                }
                cyui::endSection();
            }
            ImGui::EndDisabled();
        }
        cyui::endCard();
        bool apple=sources[source]==Format::auv2||targets[target]==Format::auv2||targets[target]==Format::auv3;
        if(apple)cyui::callout(cyui::Tone::warning,cyui::tr("Apple conversion requires macOS; AUv3 also requires Xcode. Apple validation is deferred.").c_str());
        const bool identical=sources[source]==targets[target];
        if(identical)cyui::callout(cyui::Tone::warning,cyui::tr("Source and target must be different.").c_str());
        bool buildAllowed=true;
#ifndef __APPLE__
        buildAllowed=!apple;
#endif
        const bool invalid=model.busy()||tools.running()||input.empty()||output.empty()||identical;
        ImGui::BeginDisabled(invalid||!buildAllowed);
        if(cyui::primaryButton(label("Convert").c_str()))start();
        ImGui::EndDisabled();ImGui::SameLine();
        auto snapshot=model.snapshot();
        ImGui::BeginDisabled(model.busy()||snapshot.phase!=Phase::succeeded||snapshot.artifacts.empty());
        if(cyui::button(label("Open result").c_str())&&!openUrl(fileUrl(snapshot.artifacts)))cyui::notify(cyui::tr("Cannot open the result folder.").c_str(),cyui::Tone::error);
        ImGui::EndDisabled();
        const char* phase="Ready";
        switch(snapshot.phase){case Phase::generating:phase="Preparing conversion…";break;case Phase::building:phase="Building…";break;case Phase::succeeded:phase="Completed";break;case Phase::failed:phase="Failed";break;default:break;}
        cyui::space(8);cyui::badge(cyui::tr(phase).c_str(),snapshot.phase==Phase::failed?cyui::Tone::error:snapshot.phase==Phase::succeeded?cyui::Tone::success:cyui::Tone::neutral);
        if(model.busy())cyui::progressBar(-1,cyui::dp(-1,4));
        if(!snapshot.error.empty())cyui::callout(cyui::Tone::error,snapshot.error.c_str());
        cyui::textDim("%s",cyui::tr("Keep the wrapper, payload, resources and notices together.").c_str());
        cyui::sectionTitle(cyui::tr("Conversion log").c_str());
        if(!snapshot.log.empty()){if(cyui::button(label("Copy log").c_str()))ImGui::SetClipboardText(snapshot.log.c_str());}
        if(snapshot.log.empty())snapshot.log=cyui::tr("No build output yet.");
        ImGui::InputTextMultiline("##log",snapshot.log.data(),snapshot.log.size()+1,{ImGui::GetContentRegionAvail().x,std::max(cyui::dp(130.f),ImGui::GetContentRegionAvail().y-cyui::dp(8.f))},ImGuiInputTextFlags_ReadOnly);
    }
};
void preferences(StudioTools& tools, bool save, const fs::path& path = settingsFile()) {
    if(path.empty())return;
    if(save) {
        std::error_code error;fs::create_directories(path.parent_path(),error);if(error)return;
        std::ofstream out(path);out<<"locale="<<cyui::locale()<<"\ntheme="<<cyui::currentThemeKey()<<'\n';
        out<<"mcp_access_profile="<<mcpAccessOption(tools.accessProfile()).id<<'\n';
    }else {
        std::ifstream in(path);std::string line;
        auto profile=defaultMcpAccess;
        while(std::getline(in,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            if(line.rfind("locale=",0)==0)cyui::setLocale(line.substr(7).c_str());
            if(line.rfind("theme=",0)==0)cyui::setTheme(line.substr(6).c_str());
            constexpr std::string_view key="mcp_access_profile=";
            if(line.starts_with(key))profile=parseMcpAccess(std::string_view(line).substr(key.size()));
        }
        tools.setAccessProfile(profile);
    }
}
#ifdef CYBRIDGE_STUDIO_LIVE_TEST
class LiveMcpTest {
    std::string fixture_;
    fs::path work_;
    cymcp::Cancellation cancel_;
    std::future<int> client_;
    bool controlled_ = false;

public:
    LiveMcpTest(Studio& studio, const std::vector<std::string>& arguments)
        : fixture_(arguments[2]),
          work_(pathFromText(arguments[3])/pathFromText(cymcp::random_token(16).substr(0,12))) {
        studio.chatVisible=true;
        studio.cmake=arguments[4];
        const auto& profile=arguments[5];
        if(studio.tools.accessProfile()!=defaultMcpAccess)
            throw std::runtime_error("Unexpected default MCP profile");
        fs::create_directories(work_);
        const auto ini=work_/"Studio.ini";
        {
            std::ofstream prior(ini);
            prior<<"locale=fr\ntheme=mid-grey\n";
            if(profile!="free")prior<<"mcp_access_profile="<<profile<<'\n';
        }
        preferences(studio.tools,false,ini);
        if(mcpAccessOption(studio.tools.accessProfile()).id!=profile||cyui::locale()!="fr")
            throw std::runtime_error("Studio preferences did not preserve the access profile");
        preferences(studio.tools,true,ini);
        std::ifstream saved(ini);
        std::string line;
        const auto stored="mcp_access_profile="+profile;
        while(std::getline(saved,line)&&line!=stored){}
        if(line!=stored)throw std::runtime_error("Studio did not save the selected profile");
        controlled_=cymcp::local_access_settings(studio.tools.accessProfile()).control_operations;
        studio.tools.startServer();
        cymcp::ProcessOptions process;
        process.executable=pathFromText(arguments[1]);
        process.arguments={"--live-client",fixture_,pathText(work_),profile};
        if(arguments.size()==7)process.arguments.push_back(arguments[6]);
        const auto connection=cymcp::Json{
            {"url", studio.tools.endpoint()}, {"token", studio.tools.token()}
        }.dump()+"\n";
        cancel_.deadline=cymcp::Clock::now()+std::chrono::minutes(7);
        client_=std::async(std::launch::async, [process, connection, cancel=cancel_] {
            cymcp::Process child(process);
            child.write(connection,cancel);
            child.close_input();
            cymcp::ProcessLine line;
            while(child.next(line,cancel))
                std::cerr<<line.text<<'\n';
            return child.wait(cancel);
        });
    }

    ~LiveMcpTest() { cancel_.cancel(); }
    fs::path workRoot() const { return work_/"work"; }

    std::optional<int> step(Studio& studio) {
        using namespace std::chrono_literals;
        if(client_.wait_for(0s)==std::future_status::ready)return client_.get();
        if(const auto job=studio.tools.pending()) {
            const auto& args=job->arguments;
            // Test-only responses to the native approval view, limited to the fixture
            // and the two paths generated by this test below its work directory.
            if(!controlled_||!job->external||
               pathFromText(args.at("input_path").get<std::string>())!=pathFromText(fixture_))
                return 10;
            if(job->operation=="convert_plugin") {
                const auto output=pathFromText(args.at("output_folder").get<std::string>());
                if(output!=work_/"approved"&&output!=work_/"declined")return 10;
                if(output==work_/"declined") {
                    studio.tools.decline();
                    return {};
                }
            }
            studio.approveRequest(workRoot());
        }
        return {};
    }
};
#endif
bool capture(const fs::path& path,int width,int height) {
    if(width<=0||height<=0||width>8192||height>8192)return false;
    const auto stride=(static_cast<std::uint32_t>(width)*3+3)&~3u;
    std::vector<unsigned char> rgb(static_cast<std::size_t>(width)*height*3),bgr(static_cast<std::size_t>(stride)*height);
    glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,rgb.data());
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){auto i=(static_cast<std::size_t>(y)*width+x)*3,j=static_cast<std::size_t>(y)*stride+x*3;bgr[j]=rgb[i+2];bgr[j+1]=rgb[i+1];bgr[j+2]=rgb[i];}
    std::ofstream out(path,std::ios::binary);auto put=[&out](std::uint32_t n,int bytes){for(int i=0;i<bytes;++i)out.put(static_cast<char>(n>>(8*i)));};
    out.write("BM",2);put(54+static_cast<std::uint32_t>(bgr.size()),4);put(0,4);put(54,4);put(40,4);put(width,4);put(height,4);put(1,2);put(24,2);put(0,4);put(static_cast<std::uint32_t>(bgr.size()),4);for(int i=0;i<4;++i)put(0,4);
    out.write(reinterpret_cast<const char*>(bgr.data()),static_cast<std::streamsize>(bgr.size()));return static_cast<bool>(out);
}
int run(const std::vector<std::string>& args) {
    if(args.size()==5&&args[0]=="--convert-test") {
        ConversionRequest request;request.source=parseFormat(args[1]);request.target=parseFormat(args[2]);
        request.input=pathFromText(args[3]);request.projectDirectory=pathFromText(args[4]);
        request.backend=backendFromRoot(cybridge::app::backendRoot());
        auto options=cybridge::app::buildOptions();
#ifdef __APPLE__
        if(request.target==Format::auv3)options.generator="Xcode";
#endif
        Model model;auto settings=settingsFile();
        auto cache=settings.empty()?fs::temp_directory_path()/"CyCLAPBridge":settings.parent_path();
        model.convert(std::move(request),options,cache/"Work");
        while(model.busy())std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto result=model.snapshot();std::cout<<result.log;
        if(result.phase!=Phase::succeeded){std::cerr<<result.error<<'\n';return 1;}
        std::cout<<"Output: "<<pathText(result.artifacts)<<'\n';return 0;
    }
    bool smoke=false,renderTest=false,chatView=false;std::string theme,locale;fs::path image;
    bool liveTest=false;
#ifdef CYBRIDGE_STUDIO_LIVE_TEST
    liveTest=(args.size()==6||args.size()==7)&&args[0]=="--mcp-live-test";
#endif
    for(std::size_t i=0;!liveTest&&i<args.size();++i) {
        if(args[i]=="--smoke-test")smoke=true;
        else if(args[i]=="--render-test")renderTest=true;
        else if(args[i]=="--chat")chatView=true;
        else if((args[i]=="--capture"||args[i]=="--theme"||args[i]=="--locale")&&i+1<args.size()) {
            auto key=args[i++];if(key=="--capture")image=pathFromText(args[i]);else if(key=="--theme")theme=args[i];else locale=args[i];
        }else throw std::runtime_error("Unknown or incomplete Studio option: "+args[i]);
    }
    IMGUI_CHECKVERSION();ImGui::CreateContext();
    struct Context{~Context(){ImGui::DestroyContext();}}context;
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
    translations();cyui::Config config;config.density=cyui::Density::comfortable;cyui::apply(config);
    cyui::loadFonts();
    Studio studio;
    if(!smoke&&!renderTest&&!liveTest&&image.empty())preferences(studio.tools,false);
    if(!theme.empty()&&!cyui::setTheme(theme.c_str()))throw std::runtime_error("Unknown theme");
    if(!locale.empty()&&!cyui::setLocale(locale.c_str()))throw std::runtime_error("Unknown language");
    studio.chatVisible=chatView;
    if(smoke) {
        cyui::setLocale("fr");
        unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
        io.DisplaySize={1280,900};io.DeltaTime=1.f/60;
        studio.chatVisible=true;
        if(!studio.chat.exerciseDemo([&]{
            ImGui::NewFrame();studio.draw();ImGui::Render();
        }))return 9;
        for(const bool chatPane:{false,true})
        for(const auto& skin:cyui::builtInThemes())for(const auto* language:{"en","fr"}) {
            studio.chatVisible=chatPane;
            cyui::setTheme(skin.key.c_str());cyui::setLocale(language);ImGui::NewFrame();studio.draw();ImGui::Render();
            if(!ImGui::GetDrawData()||ImGui::GetDrawData()->TotalVtxCount==0)return 3;
        }
        return 0;
    }
    glfwSetErrorCallback([](int code,const char* message){std::cerr<<"GLFW "<<code<<": "<<message<<'\n';});
    if(!glfwInit())return 4;
    struct Glfw{~Glfw(){glfwTerminate();}}glfw;
#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,2);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GL_TRUE);
    const char* glsl="#version 150";
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,0);
    const char* glsl="#version 130";
#endif
    if(renderTest||liveTest||!image.empty())glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    auto* window=glfwCreateWindow(1100,880,"CyCLAPBridge Studio",nullptr,nullptr);if(!window)return 5;
    struct Window{GLFWwindow* p;~Window(){glfwDestroyWindow(p);}}windowOwner{window};
    glfwSetWindowSizeLimits(window,760,720,GLFW_DONT_CARE,GLFW_DONT_CARE);
    glfwMakeContextCurrent(window);glfwSwapInterval(1);
    if(!ImGui_ImplGlfw_InitForOpenGL(window,true))return 6;
    struct GlfwBackend{~GlfwBackend(){ImGui_ImplGlfw_Shutdown();}}glfwBackend;
    if(!ImGui_ImplOpenGL3_Init(glsl))return 7;
    struct Renderer{~Renderer(){ImGui_ImplOpenGL3_Shutdown();}}renderer;
    fs::path workOverride;
#ifdef CYBRIDGE_STUDIO_LIVE_TEST
    std::unique_ptr<LiveMcpTest> live;
    if(liveTest) {
        live=std::make_unique<LiveMcpTest>(studio,args);
        workOverride=live->workRoot();
    }
#endif
    int frames=0;bool wrote=image.empty();
    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        float x,y;glfwGetWindowContentScale(window,&x,&y);cyui::applyDpi(x);
        ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();studio.draw(workOverride);ImGui::Render();
        int width,height;glfwGetFramebufferSize(window,&width,&height);
        if(width>0&&height>0){
            glViewport(0,0,width,height);auto color=cyui::vec(cyui::palette().bg0);glClearColor(color.x,color.y,color.z,1);glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            if(++frames>=5&&!image.empty())wrote=capture(image,width,height);
            glfwSwapBuffers(window);
        }else std::this_thread::sleep_for(std::chrono::milliseconds(20));
#ifdef CYBRIDGE_STUDIO_LIVE_TEST
        if(live) {
            if(const auto done=live->step(studio))return *done;
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
#endif
        if((renderTest||!image.empty())&&frames>=5)break;
        if(glfwWindowShouldClose(window)&&(studio.model.busy()||studio.tools.running())){
            glfwSetWindowShouldClose(window,GLFW_FALSE);cyui::notify(cyui::tr("Build in progress. Wait before closing.").c_str(),cyui::Tone::warning);
        }
    }
    if(!renderTest&&!liveTest&&image.empty())preferences(studio.tools,true);
    return wrote?0:8;
}
}
#ifdef _WIN32
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    int argc=0;auto** argv=CommandLineToArgvW(GetCommandLineW(),&argc);std::vector<std::string> args;
    if(!argv)return 1;
    for(int i=1;i<argc;++i){auto value=fs::path(argv[i]).u8string();args.emplace_back(reinterpret_cast<const char*>(value.data()),value.size());}
    LocalFree(argv);
    try{return run(args);}catch(const std::exception& e){if(args.empty())MessageBoxA(nullptr,e.what(),"CyCLAPBridge Studio",MB_OK|MB_ICONERROR);else std::cerr<<e.what()<<'\n';return 1;}
}
#else
int main(int argc,char** argv) {
    try{return run(std::vector<std::string>(argv+1,argv+argc));}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
#endif
