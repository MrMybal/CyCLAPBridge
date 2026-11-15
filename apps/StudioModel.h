// SPDX-License-Identifier: MIT
#pragma once
#include <cyclapbridge/Conversion.h>
#include <atomic>
#include <mutex>
#include <thread>
namespace cybridge::studio {
enum class Phase { ready, generating, building, succeeded, failed };
struct Snapshot {
    Phase phase = Phase::ready;
    std::string log, error;
    std::filesystem::path artifacts;
};
// UI/controller code only. No plugin is inspected while creating a project.
class Model {
public:
    ~Model();
    bool start(ConversionRequest, bool compile, BuildOptions);
    // Application workflow: build privately, then deliver runtime artifacts only.
    bool convert(ConversionRequest, BuildOptions, const std::filesystem::path& workRoot);
    bool busy() const noexcept { return running_.load(std::memory_order_acquire); }
    Snapshot snapshot() const;
private:
    void append(const std::string&);
    mutable std::mutex mutex_;
    Snapshot state_;
    std::thread worker_;
    std::atomic<bool> running_{false};
};
}
