// SPDX-License-Identifier: MIT
#include "StudioTools.h"
#include <cyclapbridge/Plugin.h>
#include <algorithm>
#include <chrono>

namespace cybridge::studio {
namespace {
using cymcp::Error;
using cymcp::Json;
namespace fs = std::filesystem;

Json stringField(const char* description, int maxLength = 1024) {
    return {{"type", "string"}, {"minLength", 1}, {"maxLength", maxLength},
            {"description", description}};
}

Json objectSchema(Json properties, Json required) {
    return {{"type", "object"}, {"properties", std::move(properties)},
            {"required", std::move(required)}, {"additionalProperties", false}};
}

Json formatField(const char* description, bool target = false) {
    auto field = stringField(description, 4);
    field["enum"] = {"clap", "vst3", "lv2", "auv2"};
    field["enum"].push_back(target ? "auv3" : "jsfx");
    return field;
}

Json listedFormat(const char* description) {
    auto field = formatField(description);
    field["enum"].push_back("auv3");
    return field;
}

Json formatCatalogueSchema() {
    const auto direction = objectSchema({
        {"source", formatField("Input format.")},
        {"target", listedFormat("Output format.")},
        {"available", {{"type", "boolean"}}},
        {"description", {{"type", "string"}}}
    }, {"source", "target", "available", "description"});
    return objectSchema({
        {"inspection_formats", {{"type", "array"}, {"items", formatField("Inspectable format.")}}},
        {"conversions", {{"type", "array"}, {"items", direction}}}
    }, {"inspection_formats", "conversions"});
}

Json jobIdField() { return stringField("Opaque Studio job ID.", 64); }

Json jobReceiptSchema() {
    return objectSchema({
        {"job_id", jobIdField()},
        {"state", {{"type", "string"}, {"const", "awaiting_approval"}}}
    }, {"job_id", "state"});
}

Json jobStatusSchema() {
    const Json text = {{"type", "string"}};
    const Json count = {{"type", "integer"}, {"minimum", 0}};
    const auto inspection = objectSchema({
        {"name", text}, {"id", text}, {"vendor", text}, {"version", text},
        {"input_buses", count}, {"output_buses", count}, {"parameters", count}
    }, {"name", "id", "vendor", "version", "input_buses", "output_buses", "parameters"});
    const auto conversion = objectSchema({
        {"output_folder", stringField("Absolute folder containing the compiled wrapper.", 4096)}
    }, {"output_folder"});
    const auto result = Json::array({Json{{"type", "null"}}, inspection, conversion});
    return objectSchema({
        {"job_id", jobIdField()},
        {"operation", {{"type", "string"}, {"enum", {"inspect_plugin", "convert_plugin"}}}},
        {"state", {{"type", "string"},
                   {"enum", {"awaiting_approval", "running", "succeeded", "failed", "declined"}}}},
        {"result", {{"anyOf", result}}}, {"error", text}, {"log", text}
    }, {"job_id", "operation", "state", "result", "error", "log"});
}

fs::path absolutePath(const Json& args, const char* key) {
    const auto value = args.at(key).get<std::string>();
    if (value.find('\0') != std::string::npos)
        throw Error("InvalidPath", std::string(key) + " contains a null byte");
    auto path = fs::path(std::u8string(value.begin(), value.end()));
    if (!path.is_absolute())
        throw Error("InvalidPath", std::string(key) + " must be an absolute path on this machine");
    return path;
}

bool canInspect(Format source) {
    if (source == Format::auv3) return false;
#ifndef __APPLE__
    if (source == Format::auv2) return false;
#endif
    return true;
}

bool availableInStudio(const Capability& entry) {
#ifndef __APPLE__
    if (entry.source == Format::auv2 || entry.target == Format::auv2 || entry.target == Format::auv3)
        return false;
#endif
    return entry.available;
}

bool canConvert(Format source, Format target) {
    return std::ranges::any_of(conversionCapabilities(), [&](const Capability& entry) {
        return entry.source == source && entry.target == target && availableInStudio(entry);
    });
}

std::string formatId(Format format) {
    auto id = formatName(format);
    for (auto& c : id)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return id;
}

Json inspect(const Json& args) {
    HostedPlugin plugin(parseFormat(args.at("source").get<std::string>()),
                        absolutePath(args, "input_path"), args.value("plugin_id", std::string{}));
    const auto& d = plugin.descriptor();
    return {{"name", d.name}, {"id", d.id}, {"vendor", d.vendor}, {"version", d.version},
            {"input_buses", plugin.inputs().size()}, {"output_buses", plugin.outputs().size()},
            {"parameters", plugin.parameters().size()}};
}
}

StudioTools::StudioTools(Model& model)
    : model_(model), registry_(std::make_shared<cymcp::Registry>(
        cymcp::Policy([](const cymcp::CallContext& ctx, const std::string&, const Json&) {
            // The HTTP transport assigns its principal only after Bearer authentication.
            // Embedded conversations use the local principal; running a job still requires UI approval.
            return ctx.principal == "local" || ctx.principal == "authenticated-local"
                ? cymcp::Decision::allow : cymcp::Decision::deny;
        }))) {
    registry_->add({"list_formats",
        "List formats that this Studio can inspect and conversion directions supported by its "
        "native build. Check available before requesting a conversion.",
        objectSchema(Json::object(), Json::array()), formatCatalogueSchema(), true,
        [](const auto&, const auto&) {
            auto formats = Json::array(), conversions = Json::array();
            for (const auto source : {Format::clap, Format::vst3, Format::lv2, Format::auv2, Format::jsfx})
                if (canInspect(source))
                    formats.push_back(formatId(source));
            for (const auto& entry : conversionCapabilities())
                conversions.push_back({{"source", formatId(entry.source)},
                    {"target", formatId(entry.target)}, {"available", availableInStudio(entry)},
                    {"description", entry.description}});
            return Json{{"inspection_formats", std::move(formats)},
                        {"conversions", std::move(conversions)}};
        }});

    Json input = {
        {"source", formatField("Input plugin or script format.")},
        {"input_path", stringField("Absolute path to the plugin file, plugin bundle, or JSFX script.", 4096)},
        {"plugin_id", stringField("Optional CLAP ID, VST3 class ID, or LV2 URI. Defaults to the first plugin.")}
    };
    registry_->add({"inspect_plugin",
        "Request inspection of one trusted plugin or JSFX script. Loads the selected input in Studio "
        "after the user approves. Returns a job ID. Use get_job to read metadata and bus/parameter counts.",
        objectSchema(input, {"source", "input_path"}), jobReceiptSchema(), false,
        [this](const auto& args, const auto& ctx) { return enqueue("inspect_plugin", args, ctx); }});

    input["target"] = formatField("Output wrapper format.", true);
    input["output_folder"] = stringField("Absolute path to a new destination folder. It must not exist.", 4096);
    input["output_name"] = stringField("Optional wrapper name. Defaults to the input name.", 120);
    input["resources_folder"] = stringField("Optional absolute source root containing the input and its imports/assets.", 4096);
    input["bundle_id"] = stringField("Optional Apple bundle identifier.", 255);
    registry_->add({"convert_plugin",
        "Request a compiled audio plugin wrapper using the Studio toolchain and the supplied local paths. "
        "The user approves the request in Studio. The new output folder receives the compiled wrapper, "
        "payload and notices. Studio never overwrites an existing folder. Use get_job with the returned ID.",
        objectSchema(input, {"source", "input_path", "target", "output_folder"}), jobReceiptSchema(), false,
        [this](const auto& args, const auto& ctx) { return enqueue("convert_plugin", args, ctx); }});

    registry_->add({"get_job",
        "Read an inspection or conversion job submitted by this channel. Returns awaiting_approval, "
        "running, succeeded, failed or declined, plus the result, error and available build log. "
        "Wait for approval or completion before polling again.",
        objectSchema({{"job_id", stringField("Job ID returned by inspect_plugin or convert_plugin.", 64)}},
                     {"job_id"}), jobStatusSchema(), true,
        [this](const Json& args, const auto& ctx) { return status(args.at("job_id").get<std::string>(), ctx); }});
}

StudioTools::~StudioTools() { stopServer(); }

Json StudioTools::enqueue(const char* operation, const Json& args, const cymcp::CallContext& context) {
    const auto source = parseFormat(args.at("source").get<std::string>());
    Json reviewed = args;
    reviewed["input_path"] = cymcp::path_utf8(absolutePath(args, "input_path"));
    if (std::string_view(operation) == "convert_plugin") {
        const auto target = parseFormat(args.at("target").get<std::string>());
        if (!canConvert(source, target))
            throw Error("Unsupported", "This conversion direction is unavailable. Use list_formats.");
        reviewed["output_folder"] = cymcp::path_utf8(absolutePath(args, "output_folder"));
        if (args.contains("resources_folder"))
            reviewed["resources_folder"] = cymcp::path_utf8(absolutePath(args, "resources_folder"));
    } else if (!canInspect(source)) {
        throw Error("Unsupported", "This inspection format is unavailable. Use list_formats.");
    }

    std::lock_guard lock(mutex_);
    context.cancellation.check();
    if (!jobs_.empty() && (jobs_.back().state == "awaiting_approval" || jobs_.back().state == "running"))
        throw Error("Busy", "Studio already has an assistant job awaiting approval or running.");
    Request request{cymcp::random_token(16), operation, std::move(reviewed),
                    context.principal == "authenticated-local"};
    jobs_.push_back({std::move(request), "awaiting_approval", nullptr, {}, {}});
    if (jobs_.size() > 8) jobs_.pop_front();
    return {{"job_id", jobs_.back().request.id}, {"state", jobs_.back().state}};
}

Json StudioTools::status(const std::string& id, const cymcp::CallContext& context) const {
    std::lock_guard lock(mutex_);
    const bool external = context.principal == "authenticated-local";
    const auto job = std::find_if(jobs_.begin(), jobs_.end(), [&](const Job& item) {
        return item.request.id == id && item.request.external == external;
    });
    if (job == jobs_.end())
        throw Error("NotFound", "Unknown or expired job ID for this channel");
    auto log = job->log;
    if (job->state == "running" && job->request.operation == "convert_plugin")
        log = model_.snapshot().log;
    return {{"job_id", id}, {"operation", job->request.operation}, {"state", job->state},
            {"result", job->result}, {"error", job->error}, {"log", std::move(log)}};
}

std::optional<StudioTools::Request> StudioTools::pending() const {
    std::lock_guard lock(mutex_);
    if (!jobs_.empty() && jobs_.back().state == "awaiting_approval")
        return jobs_.back().request;
    return {};
}

void StudioTools::approve(const BackendPaths& backend, BuildOptions options, const fs::path& workRoot) {
    std::lock_guard lock(mutex_);
    if (jobs_.empty() || jobs_.back().state != "awaiting_approval" || model_.busy())
        return;
    auto& job = jobs_.back();
    const auto& args = job.request.arguments;
    try {
        if (job.request.operation == "inspect_plugin") {
            inspection_ = std::async(std::launch::async, [args] { return inspect(args); });
        } else {
            ConversionRequest request;
            request.source = parseFormat(args.at("source").get<std::string>());
            request.target = parseFormat(args.at("target").get<std::string>());
            request.input = absolutePath(args, "input_path");
            request.projectDirectory = absolutePath(args, "output_folder");
            request.outputName = args.value("output_name", std::string{});
            request.pluginId = args.value("plugin_id", std::string{});
            request.bundleId = args.value("bundle_id", request.bundleId);
            request.backend = backend;
            if (args.contains("resources_folder"))
                request.resources = absolutePath(args, "resources_folder");
#ifdef __APPLE__
            if (request.target == Format::auv3 && options.generator.empty()) options.generator = "Xcode";
#endif
            if (!model_.convert(std::move(request), std::move(options), workRoot))
                return;
        }
        job.state = "running";
    } catch (const std::exception& e) {
        job.state = "failed";
        job.error = e.what();
    }
}

void StudioTools::decline() {
    std::lock_guard lock(mutex_);
    if (!jobs_.empty() && jobs_.back().state == "awaiting_approval")
        jobs_.back().state = "declined";
}

bool StudioTools::running() const {
    std::lock_guard lock(mutex_);
    return !jobs_.empty() && jobs_.back().state == "running";
}

void StudioTools::finish(Json result, std::string error, std::string log) {
    std::lock_guard lock(mutex_);
    auto& job = jobs_.back();
    job.state = error.empty() ? "succeeded" : "failed";
    job.result = std::move(result);
    job.error = std::move(error);
    job.log = std::move(log);
}

void StudioTools::poll() {
    if (!running()) return;
    if (inspection_.valid()) {
        if (inspection_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            return;
        try {
            finish(inspection_.get());
        } catch (const std::exception& e) {
            finish(nullptr, e.what());
        } catch (...) {
            finish(nullptr, "Unexpected inspection failure");
        }
        return;
    }
    if (!model_.busy()) {
        auto snapshot = model_.snapshot();
        auto result = snapshot.phase == Phase::succeeded
            ? Json{{"output_folder", cymcp::path_utf8(snapshot.artifacts)}} : Json(nullptr);
        finish(std::move(result), std::move(snapshot.error), std::move(snapshot.log));
    }
}

void StudioTools::startServer() {
    if (server_) return;
    auto token = cymcp::random_token();
    auto mcp = std::make_shared<cymcp::McpServer>(*registry_, "CyCLAPBridge Studio");
    auto server = std::make_unique<cymcp::LocalHttpServer>(
        token, [mcp, registry = registry_](const Json& request, const cymcp::CallContext& context) {
            const auto method = request.value("method", std::string{});
            if (method == "invoke" || method == "describe")
                return cymcp::dispatch_app(*registry, request, context);
            return mcp->handle(request, context);
        });
    endpoint_ = "http://127.0.0.1:" + std::to_string(server->start()) + "/mcp";
    token_ = std::move(token);
    server_ = std::move(server);
}

void StudioTools::stopServer() {
    server_.reset();
    endpoint_.clear();
    token_.clear();
    std::lock_guard lock(mutex_);
    std::erase_if(jobs_, [](const Job& job) {
        return job.request.external && job.state != "running";
    });
}
}
