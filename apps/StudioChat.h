// SPDX-License-Identifier: MIT
#pragma once
#include <cymcp/chat_ui.hpp>

namespace cybridge::studio {
class StudioTools;

class StudioChat {
public:
    StudioChat(StudioTools&, std::function<void()> pickAttachment);
    void draw();
    ~StudioChat();
    void poll();
    void attach(std::filesystem::path path) { panel_.add_attachments({std::move(path)}); }
    bool exerciseDemo(const std::function<void()>& render);

private:
    void select(std::size_t);
    void connect(bool restore = false);
    void drawProvider();
    void drawDiscovery();
    void drawExternalAccess();

    StudioTools& tools_;
    std::shared_ptr<cymcp::ProviderRegistry> providers_;
    cymcp::Json catalogue_;
    cymcp::ChatController controller_;
    cymcp::ChatPanel panel_;
    std::size_t selected_ = 0;
    std::string endpoint_, credentialEnv_, model_, executable_, error_;
    std::string historyId_;
    bool useTools_ = true, saveHistory_ = false;
    cymcp::Cancellation discoveryCancel_;
    std::future<std::vector<cymcp::DiscoveredConnection>> discovery_;
    std::vector<cymcp::DiscoveredConnection> detected_;
    bool detectionFinished_ = false;
};
}
