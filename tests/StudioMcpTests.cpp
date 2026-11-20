// SPDX-License-Identifier: MIT
#include "StudioTools.h"
#include <cymcp/chat_ui.hpp>
#include "demo_provider.hpp"
#include <httplib.h>
#include <algorithm>
#include <iostream>

namespace {
namespace fs = std::filesystem;
using cymcp::Json;
using namespace std::chrono_literals;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

Json call(cymcp::McpClient& client, const char* name, const Json& args) {
    const auto reply = client.call_tool(name, args);
    if (reply.value("isError", true))
        throw std::runtime_error(reply.dump());
    return reply.at("structuredContent");
}

Json waitJob(cybridge::studio::StudioTools& tools, cymcp::McpClient& client, const Json& id) {
    for (const auto end = cymcp::Clock::now() + 6min; cymcp::Clock::now() < end;) {
        tools.poll();
        const auto status = call(client, "get_job", id);
        if (status.at("state") != "running") return status;
        std::this_thread::sleep_for(150ms);
    }
    throw std::runtime_error("Timed out waiting for the Studio job");
}

Json invokeCli(const fs::path& cli, const std::string& endpoint, const std::string& token,
               int expected, std::vector<std::string> arguments) {
    cymcp::ProcessOptions process;
    process.executable = cli;
    process.arguments = std::move(arguments);
    process.arguments.insert(process.arguments.end(), {"--mcp-url", endpoint});
    process.environment.emplace("CYCLAPBRIDGE_MCP_TOKEN", token);
    cymcp::Cancellation timeout;
    timeout.deadline = cymcp::Clock::now() + 10s;
    cymcp::Process child(process);
    child.close_input();
    std::string output;
    cymcp::ProcessLine line;
    while (child.next(line, timeout))
        if (!line.stderr_stream) output += line.text;
    if (child.wait(timeout) != expected)
        throw std::runtime_error("Unexpected live CLI exit status: " + output);
    return cymcp::parse_json(output);
}

void checkLegacyClient(const std::string& endpoint, const std::string& token) {
    cymcp::HttpRequest request;
    request.method = "POST";
    request.url = endpoint;
    request.headers = {{"Content-Type", "application/json"},
                       {"Accept", "application/json, text/event-stream"}};
    if (!token.empty()) request.headers["Authorization"] = "Bearer " + token;
    request.body = R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-11-25","capabilities":{},"clientInfo":{"name":"historical-probe","version":"1"}}})";
    auto response = cymcp::HttpClient().perform(request);
    const auto initialized = cymcp::parse_json(response.body);
    require(response.status == 200 && initialized.at("result").at("protocolVersion") == "2025-11-25",
            "Legacy MCP initialize failed");
    request.headers.emplace("Mcp-Session-Id", response.headers.at("mcp-session-id"));
    request.headers.emplace("MCP-Protocol-Version", "2025-11-25");
    request.body = R"({"jsonrpc":"2.0","method":"notifications/initialized"})";
    require(cymcp::HttpClient().perform(request).status == 202, "Legacy MCP ready notification failed");
    request.body = R"({"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}})";
    response = cymcp::HttpClient().perform(request);
    require(response.status == 200 &&
            cymcp::parse_json(response.body).at("result").at("tools").size() == 4,
            "Legacy MCP catalogue was not preserved");
}

void runLiveClient(const fs::path& fixture, const fs::path& work,
                   const std::string& profile, const fs::path& cli) {
    const bool protectedAccess = profile == "protected";
    const bool controlled = protectedAccess || profile == "local_controlled";
    const bool tokenRequired = protectedAccess || profile == "token_only";
    std::string line;
    require(static_cast<bool>(std::getline(std::cin, line)), "Studio did not provide a connection");
    const auto config = cymcp::parse_json(line);
    const auto endpoint = config.at("url").get<std::string>();
    const auto token = config.at("token").get<std::string>();
    require(!token.empty() == tokenRequired, "Studio used the wrong transport authentication");
    checkLegacyClient(endpoint, token);
    cymcp::HttpRequest anonymous;
    anonymous.method = "POST";
    anonymous.url = endpoint;
    anonymous.headers = {{"Content-Type", "application/json"},
                         {"Accept", "application/json, text/event-stream"}};
    anonymous.body = R"({"jsonrpc":"2.0","id":10,"method":"initialize","params":{"protocolVersion":"2025-11-25","capabilities":{},"clientInfo":{"name":"auth-probe","version":"1"}}})";
    const auto status = cymcp::HttpClient().perform(anonymous).status;
    require(status == (tokenRequired ? 401 : 200), "Live profile applied the wrong Bearer requirement");
    if (!tokenRequired) {
        anonymous.headers["Origin"] = "https://example.invalid";
        require(cymcp::HttpClient().perform(anonymous).status == 403,
                "Tokenless server ignored its Origin policy");
    }

    cymcp::McpClient client(endpoint, token);
    client.connect();
    const auto schema = client.list_tools();
    require(schema.size() == 4, "Live Studio did not announce all four tools");
    for (const auto& tool : schema)
        require(tool.at("inputSchema").is_object() && tool.at("outputSchema").is_object(),
                "Missing input or output schema");
    const auto formats = call(client, "list_formats", Json::object());
    require(!formats.at("inspection_formats").empty(), "Live format catalogue is empty");
    const auto page = client.list_tools_page();
    const auto hits = client.cache_stats().hits;
    require(client.list_tools_page() == page && client.cache_stats().hits > hits,
            "Repeated catalogue lookup did not use the metadata cache");
    client.clear_cache();
    require(client.cache_stats().entries == 0 && client.list_tools_page() == page,
            "Explicit catalogue refresh lost the full tool listing");
    if (!cli.empty()) {
        const auto listing = invokeCli(cli, endpoint, token, 0, {"tools", "list"});
        require(listing.at("tools").size() == 4, "CLI discovery did not target live Studio");
        const auto description = invokeCli(cli, endpoint, token, 0, {"tools", "describe", "inspect_plugin"});
        require(description.at("tool").at("name") == "inspect_plugin", "CLI description mismatch");
        require(invokeCli(cli, endpoint, token, 0, {"tools", "call", "list_formats"}).at("result") == formats,
                "Live CLI and MCP catalogues differ");
        if (tokenRequired) {
            require(invokeCli(cli, endpoint, "", 4, {"tools", "list"}).contains("error"),
                    "CLI connected to a token profile with only the address");
            require(invokeCli(cli, endpoint, cymcp::random_token(), 4, {"tools", "list"}).contains("error"),
                    "CLI did not fail with an invalid token");
        }
    }

    require(client.call_tool("inspect_plugin", {{"source", "jsfx"}}).at("isError") == true,
            "Schema validation failed to reject a missing input");
    require(client.call_tool("inspect_plugin", {{"source", "jsfx"}, {"input_path", "relative.jsfx"}})
                .at("isError") == true, "A relative input path passed validation");

    const auto wait = [&](const Json& receipt) {
        require(receipt.at("state") == (controlled ? "awaiting_approval" : "queued"),
                "The live access profile applied the wrong confirmation policy");
        const Json id = {{"job_id", receipt.at("job_id")}};
        for (const auto end = cymcp::Clock::now() + 6min; cymcp::Clock::now() < end;) {
            const auto job = call(client, "get_job", id);
            if (job.at("state") != "awaiting_approval" && job.at("state") != "queued" &&
                job.at("state") != "running")
                return job;
            std::this_thread::sleep_for(150ms);
        }
        throw std::runtime_error("Live Studio did not finish the request");
    };

    Json args = {{"source", "jsfx"}, {"input_path", cymcp::path_utf8(fixture)}};
    const auto inspect = wait(call(client, "inspect_plugin", args));
    require(inspect.at("state") == "succeeded" &&
            inspect.at("result").at("parameters").get<int>() > 0,
            "Live Studio did not inspect the JSFX fixture");

    if (profile == "local_controlled" || profile == "token_only") {
        std::cout << "Live Studio access and inspection passed for " << profile << '\n';
        return;
    }

    args["target"] = "clap";
    args["output_name"] = "LiveMcpGain";
    if (controlled) {
        args["output_folder"] = cymcp::path_utf8(work / "declined");
        const auto denied = wait(call(client, "convert_plugin", args));
        require(denied.at("state") == "declined" && !fs::exists(work / "declined"),
                "A declined conversion changed the filesystem");
    }
    const auto output = work / "approved";
    args["output_folder"] = cymcp::path_utf8(output);
    const auto receipt = cli.empty() ? call(client, "convert_plugin", args)
        : invokeCli(cli, endpoint, token, 0, {"tools", "call", "convert_plugin", "--args", args.dump()}).at("result");
    const auto built = wait(receipt);
    if (built.at("state") != "succeeded") throw std::runtime_error(built.dump());
    require(built.at("result").at("output_folder") == cymcp::path_utf8(output),
            "Live Studio reported an unexpected output folder");
    require(fs::exists(output / "LiveMcpGain.clap"), "Live Studio did not produce the wrapper");
    std::cout << "Live Studio discovery, inspection and conversion passed for " << profile << '\n';
}

void checkLegacyHistory(const fs::path& archive, const fs::path& work) {
    // The input was produced by the 0.5.6 ConversationStore at revision 7010570
    // using the demo provider. Only a private copy can be rewritten by 0.7.0.
    const auto directory = work / cymcp::random_token(16);
    fs::create_directories(work);
    cymcp::private_directory(directory);
    fs::copy_file(archive, directory / "legacy-056.json");
    cymcp::ConversationStore store(directory);
    const auto before = store.load("legacy-056");

    cymcp::ChatController chat(chat_example::providers());
    cymcp::ChatConnection connection;
    connection.provider_id = "demo";
    connection.history_directory = directory;
    connection.history_id = "legacy-056";
    connection.restore = true;
    chat.open(std::move(connection));
    const auto wait = [&] {
        for (const auto end = cymcp::Clock::now() + 8s; cymcp::Clock::now() < end;) {
            chat.poll();
            if (!chat.state().busy) return;
            std::this_thread::sleep_for(10ms);
        }
        throw std::runtime_error("Historic conversation did not finish");
    };
    wait();
    const auto& restored = chat.state();
    require(restored.connected && restored.session_id == "legacy-fixture-session" &&
            restored.messages.size() == 2 && restored.messages.front().markdown == "Historic Studio fixture.",
            "Studio could not restore its 0.5.6 conversation");
    chat.send("Continued in the updated Studio.");
    wait();
    require(chat.state().messages.size() == 4 && chat.state().storage_error.empty(),
            "Restored conversation could not save a new turn");
    const auto after = store.load("legacy-056");
    const auto& oldMessages = before.at("messages");
    const auto& newMessages = after.at("messages");
    require(newMessages.size() == oldMessages.size() + 2 &&
            std::equal(oldMessages.begin(), oldMessages.end(), newMessages.begin()),
            "Resuming the conversation rewrote its original messages");
    std::cout << "0.5.6 conversation restored and extended by " << cymcp::version << '\n';
}

void checkEmbeddedChat(cybridge::studio::StudioTools& tools, const Json& input,
                       const cybridge::BackendPaths& backend, const cybridge::BuildOptions& options,
                       const fs::path& work) {
    httplib::Server service;
    service.Get("/api/tags", [](const auto&, auto& res) {
        res.set_content(R"({"models":[{"name":"studio-fixture","model":"studio-fixture"}]})",
                        "application/json");
    });
    service.Post("/api/chat", [&](const auto& req, auto& res) {
        const auto body = cymcp::parse_json(req.body);
        const auto& incoming = body.at("messages").back();
        Json message = {{"role", "assistant"}, {"content", ""}};
        if (incoming.at("role") == "tool") {
            const auto result = cymcp::parse_json(incoming.at("content").get<std::string>());
            require(result.at("state") == "awaiting_approval" && !result.at("job_id").empty(),
                    "Assistant did not receive the Studio job ID");
            message["content"] = "Studio is waiting for approval.";
        } else {
            const auto& catalogue = body.at("tools");
            require(catalogue.size() == 4, "The embedded provider did not receive the full Studio catalogue");
            const auto tool = std::find_if(catalogue.begin(), catalogue.end(), [](const Json& row) {
                return row.at("function").at("name") == "inspect_plugin";
            });
            require(tool != catalogue.end(), "Embedded model did not receive Studio tools");
            message["tool_calls"] = Json::array({
                {{"function", {{"name", "inspect_plugin"}, {"arguments", input}}}}
            });
        }
        const auto data = Json{{"message", message}, {"done", false}}.dump() + "\n" +
                          Json{{"message", {{"content", ""}}}, {"done", true},
                               {"done_reason", "stop"}}.dump() + "\n";
        res.set_content(data, "application/x-ndjson");
    });
    const auto port = service.bind_to_any_port("127.0.0.1");
    require(port > 0, "Could not bind the model fixture");
    std::thread listener([&] { service.listen_after_bind(); });
    struct Stop {
        httplib::Server& service;
        std::thread& listener;
        ~Stop() { service.stop(); listener.join(); }
    } stop{service, listener};
    service.wait_until_ready();

    cymcp::ChatController chat;
    cymcp::ChatConnection connection;
    connection.provider_id = "ollama";
    connection.provider.settings = {{"base_url", "http://127.0.0.1:" + std::to_string(port)},
                                    {"max_tool_rounds", 2}, {"timeout_ms", 3000}, {"deferred_tools", false}};
    connection.provider.tools = tools.registry();
    connection.automatic_model = true;
    chat.open(std::move(connection));
    const auto wait = [&] {
        for (const auto end = cymcp::Clock::now() + 8s; cymcp::Clock::now() < end;) {
            chat.poll();
            if (!chat.state().busy) return;
            std::this_thread::sleep_for(10ms);
        }
        throw std::runtime_error("Embedded chat did not finish");
    };
    wait();
    require(chat.state().connected, "Embedded provider did not connect");
    chat.send("Inspect the fixture.");
    wait();
    require(!chat.state().messages.empty() &&
            chat.state().messages.back().markdown == "Studio is waiting for approval.",
            "Embedded chat did not receive the tool result");
    const auto pending = tools.pending();
    require(pending && !pending->external && pending->arguments == input,
            "Assistant did not queue the requested inspection");
    tools.approve(backend, options, work);
    const Json id{{"job_id", pending->id}};
    for (const auto end = cymcp::Clock::now() + 8s; cymcp::Clock::now() < end;) {
        tools.poll();
        const auto status = tools.registry()->invoke("get_job", id);
        if (status.at("state") != "running") {
            require(status.at("state") == "succeeded" &&
                    status.at("result").at("parameters").get<int>() > 0,
                    "Embedded chat did not inspect the actual JSFX fixture");
            return;
        }
        std::this_thread::sleep_for(10ms);
    }
    throw std::runtime_error("Embedded chat inspection did not finish");
}

void run(const cybridge::BackendPaths& backend, const fs::path& fixture,
         const fs::path& work, const fs::path& cmake) {
    using namespace cybridge::studio;
    Model model;
    StudioTools tools(model);
    require(tools.endpoint().empty(), "MCP must be off at startup");
    require(tools.setAccessProfile(cymcp::LocalAccessProfile::protected_access),
            "Cannot select the protected profile");
    tools.startServer();
    cymcp::HttpRequest unauthenticated;
    unauthenticated.method = "POST";
    unauthenticated.url = tools.endpoint();
    unauthenticated.body = R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})";
    unauthenticated.headers["Content-Type"] = "application/json";
    require(cymcp::HttpClient().perform(unauthenticated).status == 401, "MCP accepted an unauthenticated client");

    cymcp::McpClient client(tools.endpoint(), tools.token());
    client.connect();
    const auto catalogue = client.list_tools();
    require(catalogue.size() == 4, "Incomplete MCP tool catalogue");
    const auto externalFormats = call(client, "list_formats", Json::object());
    const auto chatFormats = tools.registry()->invoke("list_formats", Json::object());
    require(externalFormats == chatFormats, "Chat and external MCP expose different formats");
#ifndef __APPLE__
    for (const auto& direction : externalFormats.at("conversions"))
        if (direction.at("source") == "auv2" || direction.at("target") == "auv2" ||
            direction.at("target") == "auv3")
            require(direction.at("available") == false, "Apple directions advertised on another OS");
#endif

    const Json input{{"source", "jsfx"}, {"input_path", cymcp::path_utf8(fixture)}};
    const auto chatJob = tools.registry()->invoke("inspect_plugin", input);
    const Json chatId{{"job_id", chatJob.at("job_id")}};
    require(tools.pending() && !tools.pending()->external, "Chat request skipped approval");
    require(client.call_tool("get_job", chatId).at("isError") == true, "External client read a chat job");
    tools.decline();
    require(tools.registry()->invoke("get_job", chatId).at("state") == "declined", "Declined job was not recorded");

    const auto request = call(client, "inspect_plugin", input);
    const Json inspectId{{"job_id", request.at("job_id")}};
    require(call(client, "get_job", inspectId).at("state") == "awaiting_approval", "Inspection ran early");
    cybridge::BuildOptions options;
    options.cmake = cmake;
    tools.approve(backend, options, work);
    const auto inspected = waitJob(tools, client, inspectId);
    require(inspected.at("state") == "succeeded", "Inspection failed");
    require(inspected.at("result").at("parameters").get<int>() > 0, "Missing JSFX parameters");

    auto convert = input;
    const auto output = work / ("converted-" + cymcp::random_token(16));
    convert["target"] = "clap";
    convert["output_folder"] = cymcp::path_utf8(output);
    convert["output_name"] = "McpGain";
    const Json conversionId{{"job_id", call(client, "convert_plugin", convert).at("job_id")}};
    require(!fs::exists(output) && !model.busy(), "Conversion ran before approval");
    tools.approve(backend, options, work);
    const auto completed = waitJob(tools, client, conversionId);
    if (completed.at("state") != "succeeded")
        throw std::runtime_error(completed.dump());
    require(completed.at("result").at("output_folder") == cymcp::path_utf8(output), "Wrong output folder");
    require(fs::exists(output / "McpGain.clap"), "Compiled CLAP wrapper is missing");
    require(fs::is_directory(output / "CyCLAPBridge-licenses"), "Missing wrapper notices");

    const auto oldToken = tools.token();
    tools.stopServer();
    require(tools.setAccessProfile(cymcp::LocalAccessProfile::free), "Cannot reopen as Free");
    tools.startServer();
    checkEmbeddedChat(tools, input, backend, options, work);

    tools.stopServer();
    require(tools.setAccessProfile(cymcp::LocalAccessProfile::protected_access),
            "Cannot reopen as Protected");
    tools.startServer();
    require(tools.token() != oldToken, "MCP restart reused the previous token");
    unauthenticated.url = tools.endpoint();
    unauthenticated.headers["Authorization"] = "Bearer " + oldToken;
    require(cymcp::HttpClient().perform(unauthenticated).status == 401, "MCP accepted a revoked token");
}
}

int main(int argc, char** argv) {
    try {
        if (argc == 4 && std::string_view(argv[1]) == "--restore-legacy") {
            checkLegacyHistory(fs::absolute(argv[2]), fs::absolute(argv[3]));
            return 0;
        }
        if ((argc == 5 || argc == 6) && std::string_view(argv[1]) == "--live-client") {
            runLiveClient(fs::absolute(argv[2]), fs::absolute(argv[3]), argv[4],
                          argc == 6 ? fs::absolute(argv[5]) : fs::path{});
            return 0;
        }
        if (argc != 5) return 2;
        run(cybridge::backendFromRoot(fs::absolute(argv[1])), fs::absolute(argv[2]),
            fs::absolute(argv[3]), fs::absolute(argv[4]));
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
