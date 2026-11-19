// SPDX-License-Identifier: MIT
#pragma once
#include "StudioModel.h"
#include <cymcp/mcp.hpp>
#include <optional>

namespace cybridge::studio {
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
    void decline();
    void poll();
    bool running() const;

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
    void finish(cymcp::Json, std::string error = {}, std::string log = {});

    Model& model_;
    mutable std::mutex mutex_;
    std::deque<Job> jobs_;
    std::future<cymcp::Json> inspection_;
    std::shared_ptr<cymcp::Registry> registry_;
    std::unique_ptr<cymcp::LocalHttpServer> server_;
    std::string endpoint_, token_;
};
}
