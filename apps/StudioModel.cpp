// SPDX-License-Identifier: MIT
#include "StudioModel.h"
#include <exception>
#include <stdexcept>
namespace cybridge::studio {
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
}
