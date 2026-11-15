// SPDX-License-Identifier: MIT
#include "StudioModel.h"
#include "StudioPlatform.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
using namespace cybridge;
using namespace cybridge::studio;
namespace fs=std::filesystem;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
Snapshot complete(Model& model) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
    while(model.busy()&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(2));
    require(!model.busy(),"Conversion did not finish");return model.snapshot();
}
int run(const fs::path& root,const fs::path& fixture,const fs::path& work) {
    fs::create_directories(work);
    const auto unique=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto base=work/("studio "+unique);
    fs::create_directories(base);
    const auto original=pathFromText("Plugin été #1.clap");
    fs::copy_file(fixture,base/original);
    ConversionRequest request;request.input=base/original;request.projectDirectory=base/"wrapper";
    request.outputName="Studio Gain";request.backend=backendFromRoot(root);
    Model model;require(model.start(request,false,{}),"First request rejected");
    auto result=complete(model);require(result.phase==Phase::succeeded,"Studio generation failed");
    require(fs::is_regular_file(result.artifacts/"CMakeLists.txt"),"No conversion project");
    // The controller reports overwrite refusal, then can handle another request.
    require(model.start(request,false,{}),"Retry rejected");
    result=complete(model);require(result.phase==Phase::failed&&!result.error.empty(),"Existing output was accepted");
    request.projectDirectory=base/"next";require(model.start(request,false,{}),"Recovery request rejected");
    require(complete(model).phase==Phase::succeeded,"Controller did not recover");
    // App conversion refuses existing destinations and cleans failed build work.
    const auto privateWork=base/"private-work";
    require(model.convert(request,{},privateWork),"App request rejected");
    require(complete(model).phase==Phase::failed,"Existing app output was accepted");
    require(fs::is_regular_file(request.projectDirectory/"CMakeLists.txt"),"Existing output was modified");
    request.projectDirectory=base/"runtime-output";
    BuildOptions invalidTools;invalidTools.cmake=base/"missing-cmake";
    require(model.convert(request,invalidTools,privateWork),"Failure-path request rejected");
    require(complete(model).phase==Phase::failed,"Missing toolchain unexpectedly succeeded");
    require(!fs::exists(request.projectDirectory),"Failed conversion left a partial result");
    require(fs::is_empty(privateWork),"Private build workspace was not cleaned");
    const auto url=fileUrl(base/pathFromText("Aide été #1.html"));
    require(url.rfind("file:///",0)==0,"Invalid file URL scheme");
    require(url.find(' ')==std::string::npos&&url.find('#')==std::string::npos,"Unescaped path characters in help URL");
    require(url.find("%20")!=std::string::npos&&url.find("%23")!=std::string::npos,"Lost path escaping");
    require(!openUrl("https://example.invalid"),"External URL unexpectedly accepted");
    require(!roots().empty(),"No filesystem roots");
    std::cout<<"Studio controller, Unicode paths, recovery and help URLs passed.\n";return 0;
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){try{require(argc==4,"Need root, fixture and work directory");return run(argv[1],argv[2],argv[3]);}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
#else
int main(int argc,char** argv){try{require(argc==4,"Need root, fixture and work directory");return run(argv[1],argv[2],argv[3]);}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
#endif
