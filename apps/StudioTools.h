// SPDX-License-Identifier: MIT
#pragma once
#include "StudioModel.h"
#include <cymcp/mcp.hpp>
#include <array>
#include <optional>
#include <string_view>

namespace cybridge::studio {
struct McpAccessOption {
    cymcp::LocalAccessProfile profile;
    std::string_view id;
    const char* label;
};

inline constexpr auto defaultMcpAccess = cymcp::LocalAccessProfile::free;
inline constexpr std::array mcpAccessOptions{
    McpAccessOption{cymcp::LocalAccessProfile::free, "free", "Free"},
    McpAccessOption{cymcp::LocalAccessProfile::local_controlled, "local_controlled", "Local controlled"},
    McpAccessOption{cymcp::LocalAccessProfile::token_only, "token_only", "Token only"},
    McpAccessOption{cymcp::LocalAccessProfile::protected_access, "protected", "Protected"}
};
const McpAccessOption& mcpAccessOption(cymcp::LocalAccessProfile);
cymcp::LocalAccessProfile parseMcpAccess(std::string_view);

class StudioTools {
public:
    struct Request {
        std::string id, operation;
        cymcp::Json arguments;
        bool external;
    };

    explicit StudioTools(Model&);
    ~StudioTools();
    std::shared_ptr<const cymcp::Registry> registry() const { return registry_; }
    std::optional<Request> pending() const;
    void approve(const BackendPaths&, BuildOptions, const std::filesystem::path& workRoot);
    bool queued() const;
    void startQueued(const BackendPaths&, BuildOptions, const std::filesystem::path& workRoot);
    void failQueued(const std::string& error);
    void decline();
    void poll();
    bool running() const;

    cymcp::LocalAccessProfile accessProfile() const { return profile_; }
    bool setAccessProfile(cymcp::LocalAccessProfile);
    void startServer();
    void stopServer();
    const std::string& endpoint() const { return endpoint_; }
    const std::string& token() const { return token_; }

private:
    struct Job {
        Request request;
        std::string state = "awaiting_approval";
        cymcp::Json result = nullptr;
        std::string error, log;
    };

    cymcp::Json enqueue(const char*, const cymcp::Json&, const cymcp::CallContext&);
    cymcp::Json status(const std::string&, const cymcp::CallContext&) const;
    void startWaiting(std::string_view state, const BackendPaths&, BuildOptions,
                      const std::filesystem::path& workRoot);
    void finish(cymcp::Json, std::string error = {}, std::string log = {});

    Model& model_;
    mutable std::mutex mutex_;
    std::deque<Job> jobs_;
    std::future<cymcp::Json> inspection_;
    std::shared_ptr<cymcp::Registry> registry_;
    // Set on the UI thread only while HTTP is stopped. External callbacks complete
    // before stopServer returns, and embedded chat does not read this profile.
    cymcp::LocalAccessProfile profile_ = defaultMcpAccess;
    std::unique_ptr<cymcp::LocalHttpServer> server_;
    std::string endpoint_, token_;
};
}
