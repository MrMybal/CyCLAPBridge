// SPDX-License-Identifier: MIT
#include "StudioTools.h"
#include "AppPaths.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string_view>

namespace cybridge::app {
namespace {
using cymcp::Error;
using cymcp::Json;

struct Options {
    std::string command, name, url, tokenEnv = "CYCLAPBRIDGE_MCP_TOKEN";
    Json arguments = Json::object();
};

[[noreturn]] void invalid(const char* message) {
    throw Error("InvalidArguments", message);
}

std::string readArguments(const std::string& name) {
    const auto path = std::filesystem::path(std::u8string(name.begin(), name.end()));
    std::ifstream file(path, std::ios::binary);
    if (!file) invalid("Cannot open the JSON arguments file.");
    constexpr std::size_t limit = 1024 * 1024;
    std::string data(limit + 1, '\0');
    file.read(data.data(), static_cast<std::streamsize>(data.size()));
    data.resize(static_cast<std::size_t>(file.gcount()));
    if (file.bad() || data.size() > limit)
        invalid("Cannot read the arguments file or it exceeds 1 MiB.");
    if (data.starts_with("\xef\xbb\xbf")) data.erase(0, 3);
    return data;
}

Options parse(const std::vector<std::string>& args) {
    if (args.size() < 3) invalid("Use tools list, tools describe NAME, or tools call NAME.");
    Options request;
    request.command = args[2];
    std::size_t next = 3;
    if (request.command == "describe" || request.command == "call") {
        if (next == args.size() || args[next].empty() || args[next].starts_with("--"))
            invalid("A tool name is required.");
        request.name = args[next++];
    } else if (request.command != "list") {
        invalid("Unknown tools command.");
    }

    bool hasArguments = false, hasTokenEnv = false;
    while (next < args.size()) {
        const auto& option = args[next++];
        if (next == args.size()) invalid("A tools option is missing its value.");
        const auto& value = args[next++];
        if (value.empty()) invalid("A tools option has an empty value.");
        if (option == "--mcp-url" && request.url.empty()) {
            request.url = value;
        } else if (option == "--mcp-token-env" && !hasTokenEnv) {
            if (value.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_")
                != std::string::npos)
                invalid("Invalid token environment variable name.");
            request.tokenEnv = value;
            hasTokenEnv = true;
        } else if ((option == "--args" || option == "--args-file") &&
                   request.command == "call" && !hasArguments) {
            request.arguments = cymcp::parse_json(option == "--args" ? value : readArguments(value));
            if (!request.arguments.is_object()) invalid("Tool arguments must be a JSON object.");
            hasArguments = true;
        } else {
            invalid("Unknown, duplicate, or inapplicable tools option.");
        }
    }
    if (hasTokenEnv && request.url.empty())
        invalid("--mcp-token-env requires --mcp-url.");
    return request;
}

std::string tokenFromEnvironment(const std::string& name) {
#ifdef _WIN32
    char* data = nullptr;
    std::size_t size = 0;
    const auto error = _dupenv_s(&data, &size, name.c_str());
    const std::unique_ptr<char, decltype(&std::free)> value(data, &std::free);
    if (!error && value) return value.get();
#else
    if (const auto* value = std::getenv(name.c_str())) return value;
#endif
    return {};
}

void output(const Json& result) {
    std::cout << result.dump(-1, ' ', false, Json::error_handler_t::replace) << '\n';
}

int failure(int exitCode, const std::string& code, const std::string& message) {
    output({{"error", {{"code", code}, {"message", message}}}});
    return exitCode;
}

int local(const Options& request) {
    studio::Model model;
    const studio::StudioTools tools(model);
    const auto registry = tools.registry();
    if (request.command == "list") {
        output({{"tools", registry->describe()}});
    } else if (request.command == "describe") {
        output({{"tool", registry->describe(request.name)}});
    } else {
        registry->describe(request.name);
        if (request.name != "list_formats")
            return failure(3, "StudioRequired",
                "This tool needs the current Studio instance. Pass its URL with --mcp-url.");
        output({{"result", registry->invoke(request.name, request.arguments)}});
    }
    return 0;
}

int remote(const Options& request) {
    cymcp::Url endpoint;
    try {
        endpoint = cymcp::parse_url(request.url);
    } catch (const Error&) {
        return failure(2, "InvalidEndpoint", "Use the http://127.0.0.1:PORT/mcp URL shown by Studio.");
    }
    if (endpoint.scheme != "http" || endpoint.host != "127.0.0.1" ||
        endpoint.target != "/mcp" || endpoint.port <= 0)
        return failure(2, "InvalidEndpoint", "Use the http://127.0.0.1:PORT/mcp URL shown by Studio.");
    const auto token = tokenFromEnvironment(request.tokenEnv);
    if (token.empty()) return failure(4, "MissingToken", "Set the token environment variable for Studio.");
    cymcp::McpClient client(request.url, token);
    client.connect();
    if (request.command == "list") {
        output({{"tools", client.list_tools()}});
    } else if (request.command == "describe") {
        output({{"tool", client.get_tool(request.name)}});
    } else {
        const auto reply = client.call_tool(request.name, request.arguments);
        if (reply.at("isError").get<bool>()) {
            const auto detail = cymcp::parse_json(reply.at("content").at(0).at("text").get<std::string>());
            return failure(3, detail.at("code").get<std::string>(), detail.at("message").get<std::string>());
        }
        output({{"result", reply.at("structuredContent")}});
    }
    return 0;
}
} // namespace

std::filesystem::path cliBackendRoot(const BuildOptions& options) {
    namespace fs = std::filesystem;
    const auto root = backendRoot();
    const auto archive = root / "ConversionBackend.tar.gz";
    if (!fs::is_regular_file(archive)) return root;
    const auto settings = settingsFile();
    if (settings.empty()) throw std::runtime_error("Cannot locate the user conversion cache.");
    // Generated projects refer to this immutable backend, including for a later build.
    const auto cache = settings.parent_path() / "CLIBackend";
    const auto contentId = cymcp::sha256_file(archive).substr(0, 24);
    const auto installed = cache / contentId;
    const auto ready = [](const fs::path& path) {
        return fs::is_regular_file(path / "templates" / "PayloadEntry.cpp");
    };
    if (ready(installed)) return installed;

    cymcp::private_directory(cache);
    const auto temporary = cache / ("stage-" + cymcp::random_token(16));
    if (!fs::create_directory(temporary)) throw std::runtime_error("Cannot stage the conversion backend.");
    struct Cleanup {
        fs::path path;
        ~Cleanup() { std::error_code ignored; fs::remove_all(path, ignored); }
    } cleanup{temporary};
    const auto extracted = studio::prepareBackend(backendFromRoot(root), temporary, options,
        [](const std::string& line) { std::cerr << line; });
    std::error_code result;
    fs::rename(extracted.projectRoot, installed, result);
    if (result && !ready(installed))
        throw std::runtime_error("Cannot install the conversion backend: " + result.message());
    return installed;
}

int runTools(const std::vector<std::string>& args) {
    if (args.size() == 3 && (args[2] == "--help" || args[2] == "help")) {
        std::cout << "tools list | tools describe NAME | tools call NAME [--args JSON | --args-file FILE]\n"
            "Add --mcp-url http://127.0.0.1:PORT/mcp to target an open Studio.\n"
            "The token comes from CYCLAPBRIDGE_MCP_TOKEN or --mcp-token-env VAR.\n"
            "Without a Studio URL only list_formats can run. All tool descriptions are available.\n"
            "Exit codes: 0 success, 2 arguments, 3 tool error, 4 connection/protocol error.\n";
        return 0;
    }
    Options request;
    try {
        request = parse(args);
    } catch (const std::exception& error) {
        return failure(2, "InvalidArguments", error.what());
    }
    try {
        return request.url.empty() ? local(request) : remote(request);
    } catch (const Error& error) {
        const bool unknown = error.code == "UnknownOperation" || error.code == "NotFound";
        return failure(request.url.empty() || unknown ? 3 : 4, error.code, error.what());
    } catch (const std::exception& error) {
        return failure(4, "ProtocolError", error.what());
    }
}
} // namespace cybridge::app
