// SPDX-License-Identifier: MIT
#include "StudioModel.h"
#include "../src/Process.h"
#include <chrono>
#include <exception>
#include <fstream>
#include <random>
#include <stdexcept>
namespace cybridge::studio {
namespace {
namespace fs=std::filesystem;
// Every directory removed here is freshly and exclusively created by this job.
class WorkDirectory {
public:
    fs::path path;
    explicit WorkDirectory(const fs::path& parent) {
        const auto base=fs::absolute(parent).lexically_normal();
        fs::create_directories(base);
        for(int attempt=0;attempt<8;++attempt) {
            auto candidate=base/("cybridge-job-"+std::to_string(std::random_device{}())+"-"+
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            if(candidate.parent_path()!=base)throw std::runtime_error("Invalid work directory");
            if(fs::create_directory(candidate)){path=std::move(candidate);return;}
        }
        throw std::runtime_error("Cannot create a private conversion workspace");
    }
    ~WorkDirectory(){std::error_code ignored;fs::remove_all(path,ignored);}
};
}
BackendPaths prepareBackend(const BackendPaths& backend,const fs::path& workspace,
                            const BuildOptions& options,const LogSink& log) {
    if(fs::is_regular_file(backend.templates/"PayloadEntry.cpp"))return backend;
    const auto archive=backend.projectRoot/"ConversionBackend.tar.gz";
    const auto script=backend.projectRoot/"ExtractBackend.cmake";
    if(!fs::is_regular_file(archive)||!fs::is_regular_file(script))
        throw std::runtime_error("Application conversion components are missing. Extract the complete application package.");
    const auto destination=workspace/"components";
    const auto code=detail::runProcess(options.cmake,{"-DARCHIVE:FILEPATH="+detail::utf8(archive),
        "-DOUTPUT:PATH="+detail::utf8(destination),"-P",detail::utf8(script)},log);
    if(code!=0||!fs::is_regular_file(destination/"templates/PayloadEntry.cpp"))
        throw std::runtime_error("Cannot prepare the conversion components. Check the CMake executable in Advanced options.");
    return backendFromRoot(destination);
}
namespace {
void deliver(const fs::path& artifacts,const fs::path& destination) {
    if(!fs::is_directory(artifacts))throw std::runtime_error("The build produced no artifact directory");
    WorkDirectory staging(destination.parent_path());
    const auto result=staging.path/"result";fs::create_directory(result);
    std::size_t copied=0;
    for(const auto& entry:fs::directory_iterator(artifacts)) {
        const auto extension=entry.path().extension();
        if(entry.is_regular_file()&&(extension==".pdb"||extension==".exp"||extension==".lib"||extension==".ilk"))continue;
        fs::copy(entry.path(),result/entry.path().filename(),fs::copy_options::recursive);++copied;
    }
    if(!copied)throw std::runtime_error("The build produced no runtime artifacts");
    if(fs::exists(destination))throw std::runtime_error("Output folder already exists; choose a new folder");
    fs::rename(result,destination);
}
}
Model::~Model() { if(worker_.joinable()) worker_.join(); }
Snapshot Model::snapshot() const { std::lock_guard lock(mutex_); return state_; }
void Model::append(const std::string& text) {
    std::lock_guard lock(mutex_);
    constexpr std::size_t limit = 256u * 1024u;
    state_.log.append(text);
    if(state_.log.size() > limit) {
        auto at=state_.log.size()-limit;
        while(at<state_.log.size()&&(static_cast<unsigned char>(state_.log[at])&0xc0)==0x80)++at;
        state_.log.erase(0,at);
    }
}
bool Model::start(ConversionRequest request, bool compile, BuildOptions options) {
    if(running_.exchange(true,std::memory_order_acq_rel))return false;
    if(worker_.joinable())worker_.join();
    {std::lock_guard lock(mutex_);state_={};state_.phase=Phase::generating;}
    try {
        worker_=std::thread([this,request=std::move(request),compile,options=std::move(options)] {
            try {
                auto project=createConversionProject(request);
                append("Project created.\n");
                auto artifacts=project.directory;
                if(compile) {
                    {std::lock_guard lock(mutex_);state_.phase=Phase::building;}
                    auto result=buildConversionProject(project,options,[this](const auto& text){append(text);});
                    if(result.exitCode!=0)throw std::runtime_error("Build failed (exit "+std::to_string(result.exitCode)+")");
                    artifacts=result.artifactsDirectory;
                }
                {std::lock_guard lock(mutex_);state_.artifacts=artifacts;state_.phase=Phase::succeeded;}
            }catch(const std::exception& e){std::lock_guard lock(mutex_);state_.error=e.what();state_.phase=Phase::failed;}
            catch(...){std::lock_guard lock(mutex_);state_.error="Unexpected conversion failure";state_.phase=Phase::failed;}
            running_.store(false,std::memory_order_release);
        });
    }catch(...){running_.store(false,std::memory_order_release);throw;}
    return true;
}
bool Model::convert(ConversionRequest request,BuildOptions options,const fs::path& workRoot) {
    if(running_.exchange(true,std::memory_order_acq_rel))return false;
    if(worker_.joinable())worker_.join();
    {std::lock_guard lock(mutex_);state_={};state_.phase=Phase::generating;}
    try {
        worker_=std::thread([this,request=std::move(request),options=std::move(options),workRoot]() mutable {
            try {
                if(request.projectDirectory.empty())throw std::invalid_argument("Choose a new output folder");
                const auto destination=fs::absolute(request.projectDirectory).lexically_normal();
                if(fs::exists(destination))throw std::invalid_argument("Output folder already exists; choose a new folder");
                WorkDirectory workspace(workRoot);
                const auto sink=[this](const std::string& line){append(line);};
                request.backend=prepareBackend(request.backend,workspace.path,options,sink);
                request.projectDirectory=workspace.path/"project";
                const auto project=createConversionProject(request);
                {std::lock_guard lock(mutex_);state_.phase=Phase::building;}
                const auto built=buildConversionProject(project,options,sink);
                if(built.exitCode!=0)throw std::runtime_error("Conversion failed (exit "+std::to_string(built.exitCode)+
                    "). A native C++ compiler and platform SDK are required; see the log.");
                deliver(built.artifactsDirectory,destination);
                append("Conversion completed.\n");
                {std::lock_guard lock(mutex_);state_.artifacts=destination;state_.phase=Phase::succeeded;}
            }catch(const std::exception& e){std::lock_guard lock(mutex_);state_.error=e.what();state_.phase=Phase::failed;}
            catch(...){std::lock_guard lock(mutex_);state_.error="Unexpected conversion failure";state_.phase=Phase::failed;}
            running_.store(false,std::memory_order_release);
        });
    }catch(...){running_.store(false,std::memory_order_release);throw;}
    return true;
}
}
