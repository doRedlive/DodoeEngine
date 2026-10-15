// do@Redlive

#include "McpServer.h"

#include "AICommandBridge.h"
#include "CommandRegistry.h"
#include "ExecutionRecorder.h"
#include "WorkflowTemplate.h"
#include "core/EditorSession.h"
#include "core/document/EditorDocumentModel.h"
#include "core/document/EditorDocumentSerializer.h"
#include "mcp/McpHttp.h"
#include "mcp/McpRpc.h"

#include <algorithm>
#include <chrono>
#include <utility>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#endif

namespace cakery {

namespace {

#ifdef _WIN32

constexpr const char* kMcpEndpoint = "/mcp";
constexpr DWORD kSocketTimeoutMs = 30000;
constexpr auto kToolCallTimeout = std::chrono::seconds(300);
constexpr auto kSsePollInterval = std::chrono::milliseconds(100);

bool EnsureWinsock() {
    static const bool initialized = [] {
        WSADATA data{};
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    return initialized;
}

std::string HttpResponse(int code, const char* reason, const std::string& body,
                         const std::string& extraHeaders = {}) {
    std::string head = "HTTP/1.1 ";
    head += std::to_string(code);
    head += " ";
    head += reason;
    head += "\r\nContent-Type: application/json\r\nContent-Length: ";
    head += std::to_string(body.size());
    head += "\r\nConnection: close";
    if (!extraHeaders.empty()) {
        head += "\r\n";
        head += extraHeaders;
    }
    head += "\r\n\r\n";
    return head + body;
}

bool SendAll(SOCKET socket, const std::string& payload) {
    std::size_t sent = 0;
    while (sent < payload.size()) {
        const int result = send(socket, payload.data() + sent,
                                static_cast<int>(payload.size() - sent), 0);
        if (result <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(result);
    }
    return true;
}

nlohmann::json NotificationMessage(const std::string& level, const std::string& data) {
    return nlohmann::json{{"jsonrpc", "2.0"},
                          {"method", "notifications/message"},
                          {"params", nlohmann::json{{"level", level}, {"data", data}}}};
}

nlohmann::json ProgressNotification(const nlohmann::json& token, int done, int total) {
    nlohmann::json params;
    params["progressToken"] = token;
    params["progress"] = done;
    params["total"] = total;
    params["message"] = "Benchmark frames: " + std::to_string(done) + "/" + std::to_string(total);
    return nlohmann::json{{"jsonrpc", "2.0"},
                          {"method", "notifications/progress"},
                          {"params", std::move(params)}};
}

bool PayloadRequestsInitialize(const nlohmann::json& payload) {
    if (payload.is_array()) {
        for (const auto& item : payload) {
            if (PayloadRequestsInitialize(item)) {
                return true;
            }
        }
        return false;
    }
    return payload.is_object() && payload.contains("method") &&
           payload["method"].is_string() && payload["method"].get<std::string>() == "initialize";
}

const nlohmann::json kEmptyObject = nlohmann::json::object();

const nlohmann::json& ObjectOrEmpty(const nlohmann::json& value) {
    return value.is_object() ? value : kEmptyObject;
}

nlohmann::json BuildPromptList() {
    auto argument = [](const std::string& name, const std::string& description, bool required) {
        nlohmann::json item;
        item["name"] = name;
        item["description"] = description;
        item["required"] = required;
        return item;
    };
    auto prompt = [](const std::string& name, const std::string& description,
                     nlohmann::json arguments) {
        nlohmann::json item;
        item["name"] = name;
        item["description"] = description;
        item["arguments"] = std::move(arguments);
        return item;
    };
    nlohmann::json optimizeArgs = nlohmann::json::array();
    optimizeArgs.push_back(argument("focus", "draw calls, overdraw, geometry or textures", false));
    nlohmann::json inspectArgs = nlohmann::json::array();
    inspectArgs.push_back(argument("uuid", "Entity UUID", true));

    nlohmann::json prompts = nlohmann::json::array();
    prompts.push_back(prompt("optimize_scene",
                             "Analyze the active scene and propose performance improvements",
                             std::move(optimizeArgs)));
    prompts.push_back(prompt("summarize_workflow",
                             "Summarize the recorded execution workflow graph",
                             nlohmann::json::array()));
    prompts.push_back(prompt("inspect_entity",
                             "Inspect one entity: components, field values and dependencies",
                             std::move(inspectArgs)));
    nlohmann::json out;
    out["prompts"] = std::move(prompts);
    return out;
}

std::optional<nlohmann::json> BuildPromptResult(const std::string& name,
                                                const nlohmann::json& arguments) {
    std::string description;
    std::string text;
    if (name == "optimize_scene") {
        const std::string focus = arguments.value("focus", std::string("draw calls"));
        description = "Optimize the active scene";
        text = "Use the editor tools to profile the active scene "
               "(query.perf.frame_stats, query.perf.passes, query.entities) and propose "
               "concrete optimizations focused on: " + focus +
               ". Validate each change with benchmark.run and benchmark.compare before "
               "suggesting the next one.";
    } else if (name == "summarize_workflow") {
        description = "Summarize the workflow graph";
        text = "Call workflow.graph and summarize the execution flow: sessions, node "
               "dependencies and any failed steps.";
    } else if (name == "inspect_entity") {
        const std::string uuid = arguments.value("uuid", std::string());
        if (uuid.empty()) {
            return std::nullopt;
        }
        description = "Inspect an entity";
        text = "Inspect entity " + uuid + ": call query.components for its component list, "
               "then read the relevant fields via the inspector data and report anything "
               "unusual.";
    } else {
        return std::nullopt;
    }
    nlohmann::json content;
    content["type"] = "text";
    content["text"] = text;
    nlohmann::json message;
    message["role"] = "user";
    message["content"] = std::move(content);
    nlohmann::json messages = nlohmann::json::array();
    messages.push_back(std::move(message));
    nlohmann::json result;
    result["description"] = description;
    result["messages"] = std::move(messages);
    return result;
}

#endif // _WIN32

} // namespace

#ifdef _WIN32

McpServer& McpServer::self() {
    static McpServer instance;
    return instance;
}

McpServer::~McpServer() {
    stop();
}

bool McpServer::start(EditorSession& session, std::uint16_t port) {
    stop();
    if (!EnsureWinsock()) {
        return false;
    }
    const SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        return false;
    }
    BOOL reuse = TRUE;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse),
               sizeof(reuse));
    u_long nonBlocking = 1;
    ioctlsocket(listener, FIONBIO, &nonBlocking);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0 ||
        listen(listener, 8) != 0) {
        closesocket(listener);
        return false;
    }
    sockaddr_in bound{};
    int boundLength = sizeof(bound);
    std::uint16_t actualPort = port;
    if (getsockname(listener, reinterpret_cast<sockaddr*>(&bound), &boundLength) == 0) {
        actualPort = ntohs(bound.sin_port);
    }
    AICommandBridge::self().attach(session);
    AICommandBridge::self().setDenylist({"scene.new", "scene.open", "workflow.clear",
                                         "mcp.start", "mcp.stop", "mcp.deny", "mcp.allow"});
    m_session = &session;
    m_listenSocket = static_cast<std::uintptr_t>(listener);
    m_port = actualPort;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_mcpSessionId.clear();
    }
    m_running.store(true, std::memory_order_release);
    m_acceptThread = std::thread(&McpServer::acceptLoop, this);
    return true;
}

void McpServer::stop() {
    m_running.store(false, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(m_callMutex);
        for (auto& call : m_pending) {
            call->ok = false;
            call->result = nlohmann::json{{"ok", false}, {"error", "MCP server is stopping"}};
            call->completed = true;
        }
        m_pending.clear();
    }
    m_callCv.notify_all();
#ifdef _WIN32
    if (m_listenSocket != 0) {
        closesocket(static_cast<SOCKET>(m_listenSocket));
        m_listenSocket = 0;
    }
#endif
    if (m_acceptThread.joinable()) {
        m_acceptThread.join();
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (m_activeConnections.load(std::memory_order_acquire) > 0 &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    AICommandBridge::self().detach();
    m_session = nullptr;
    m_port = 0;
}

std::size_t McpServer::pendingCount() const {
    std::lock_guard<std::mutex> lock(m_callMutex);
    return m_pending.size();
}

std::string McpServer::currentSessionId() const {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_mcpSessionId;
}

void McpServer::finishCall(const std::shared_ptr<McpToolCall>& call, bool ok, nlohmann::json result) {
    {
        std::lock_guard<std::mutex> lock(m_callMutex);
        call->ok = ok;
        call->result = std::move(result);
        call->completed = true;
    }
    m_callCv.notify_all();
}

void McpServer::pushCallEvent(const std::shared_ptr<McpToolCall>& call, nlohmann::json event) {
    {
        std::lock_guard<std::mutex> lock(m_callMutex);
        if (!call->completed) {
            call->events.push_back(std::move(event));
        }
    }
    m_callCv.notify_all();
}

void McpServer::acceptLoop() {
    while (m_running.load(std::memory_order_acquire)) {
        const SOCKET client = accept(static_cast<SOCKET>(m_listenSocket), nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            if (!m_running.load(std::memory_order_acquire)) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        std::thread(&McpServer::serveClient, this, static_cast<std::uintptr_t>(client)).detach();
    }
}

void McpServer::serveClient(std::uintptr_t socketHandle) {
    m_activeConnections.fetch_add(1, std::memory_order_release);
    const SOCKET client = static_cast<SOCKET>(socketHandle);
    DWORD timeout = kSocketTimeoutMs;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout),
               sizeof(timeout));

    auto feed = [&client](mcp::HttpBuffer& buffer) {
        char chunk[8192];
        const int received = recv(client, chunk, static_cast<int>(sizeof(chunk)), 0);
        if (received <= 0) {
            return false;
        }
        buffer.feed(chunk, static_cast<std::size_t>(received));
        return true;
    };

    std::string response;
    std::string payload;
    std::string extraHeaders;
    bool streamResponse = false;
    nlohmann::json message = nullptr;
    mcp::HttpRequestHead head;
    mcp::HttpBuffer buffer;
    std::string body;
    const bool pathOk = mcp::ParseRequestHead(buffer, head, feed) && head.valid &&
                        head.target == kMcpEndpoint;
    if (!pathOk) {
        response = HttpResponse(404, "Not Found", "{\"error\":\"not found\"}");
    } else if (head.method != "POST") {
        response = HttpResponse(405, "Method Not Allowed", "{\"error\":\"POST only\"}");
    } else if (!mcp::ReadBody(buffer, head.content_length, body, feed)) {
        response = HttpResponse(400, "Bad Request", "{\"error\":\"incomplete body\"}");
    } else {
        message = nlohmann::json::parse(body, nullptr, false);
    }

    if (response.empty() && (message.is_discarded() || message.is_null())) {
        payload = mcp::RpcError(nlohmann::json(nullptr), -32700, "Parse error").dump();
    } else if (response.empty()) {
        const std::string assigned = currentSessionId();
        if (!assigned.empty() && !PayloadRequestsInitialize(message) &&
            head.session_id != assigned) {
            response = HttpResponse(404, "Not Found", "{\"error\":\"session not found\"}");
        }
    }

    if (response.empty() && !message.is_null() && !message.is_discarded()) {
        const std::string method =
            message.is_object() && message.contains("method") && message["method"].is_string()
                ? message["method"].get<std::string>()
                : std::string();
        const std::string sessionIdBefore = currentSessionId();
        if (method == "tools/call") {
            streamResponse = true;
        } else {
            payload = processRpcPayload(message);
        }
        const std::string sessionIdAfter = currentSessionId();
        if (sessionIdBefore.empty() && !sessionIdAfter.empty()) {
            extraHeaders = "Mcp-Session-Id: " + sessionIdAfter;
        }
    }

    if (!streamResponse) {
        if (response.empty()) {
            response = payload.empty() ? HttpResponse(202, "Accepted", "")
                                       : HttpResponse(200, "OK", payload, extraHeaders);
        }
        SendAll(client, response);
    } else {
        SendAll(client,
                "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\n"
                "Connection: close\r\n\r\n");
        const nlohmann::json& params =
            message.is_object() && message.contains("params")
                ? ObjectOrEmpty(message["params"])
                : kEmptyObject;
        if (!params.contains("name") || !params["name"].is_string() ||
            !CommandRegistry::self().has(params["name"].get<std::string>())) {
            const char* reason = params.contains("name") ? "Unknown tool" : "Missing tool name";
            const nlohmann::json id =
                message.contains("id") ? message["id"] : nlohmann::json(nullptr);
            SendAll(client, "event: message\ndata: " +
                                mcp::RpcError(id, -32602, reason).dump() + "\n\n");
        } else {
            auto call = enqueueToolCall(params);
            const auto deadline = std::chrono::steady_clock::now() + kToolCallTimeout;
            pushCallEvent(call, NotificationMessage("info", "Executing " + call->name));
            while (true) {
                std::vector<nlohmann::json> events;
                bool completed = false;
                {
                    std::unique_lock<std::mutex> lock(m_callMutex);
                    m_callCv.wait_for(lock, kSsePollInterval, [&] {
                        return call->completed || !call->events.empty();
                    });
                    events.swap(call->events);
                    completed = call->completed;
                }
                for (const auto& event : events) {
                    SendAll(client, "event: message\ndata: " + event.dump() + "\n\n");
                }
                if (completed) {
                    break;
                }
                if (!m_running.load(std::memory_order_acquire) ||
                    std::chrono::steady_clock::now() > deadline) {
                    finishCall(call, false,
                               nlohmann::json{{"ok", false},
                                              {"error", "Tool call timed out or server stopping"}});
                }
            }
            const nlohmann::json result = awaitToolCall(call);
            const nlohmann::json id = message.contains("id") ? message["id"] : nlohmann::json(nullptr);
            SendAll(client,
                    "event: message\ndata: " + mcp::RpcResult(id, result).dump() + "\n\n");
        }
    }

    shutdown(client, SD_BOTH);
    closesocket(client);
    m_activeConnections.fetch_sub(1, std::memory_order_release);
}

std::string McpServer::processRpcPayload(const nlohmann::json& payload) {
    if (payload.is_array()) {
        nlohmann::json responses = nlohmann::json::array();
        for (const auto& item : payload) {
            if (auto response = handleRpc(item)) {
                responses.push_back(std::move(*response));
            }
        }
        return responses.empty() ? std::string() : responses.dump();
    }
    if (auto response = handleRpc(payload)) {
        return response->dump();
    }
    return std::string();
}

std::optional<nlohmann::json> McpServer::handleRpc(const nlohmann::json& message) {
    if (!message.is_object() || !message.contains("method") || !message["method"].is_string()) {
        const nlohmann::json id =
            message.is_object() && message.contains("id") ? message["id"] : nlohmann::json(nullptr);
        return mcp::RpcError(id, -32600, "Invalid Request");
    }
    const std::string method = message["method"].get<std::string>();
    const bool hasId = message.contains("id");
    const nlohmann::json id = hasId ? message["id"] : nlohmann::json(nullptr);

    if (method.rfind("notifications/", 0) == 0) {
        return std::nullopt;
    }
    if (method == "initialize") {
        std::string requested;
        if (message.contains("params") && message["params"].is_object()) {
            const auto it = message["params"].find("protocolVersion");
            if (it != message["params"].end() && it->is_string()) {
                requested = it->get<std::string>();
            }
        }
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (m_mcpSessionId.empty()) {
            m_mcpSessionId = std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count());
        }
        return mcp::RpcResult(id, mcp::BuildInitializeResult(requested));
    }
    if (method == "ping") {
        return mcp::RpcResult(id, nlohmann::json::object());
    }
    if (method == "tools/list") {
        return mcp::RpcResult(id, mcp::BuildToolCatalog(CommandRegistry::self().toolSchema()));
    }
    if (method == "tools/call") {
        const nlohmann::json& params =
            message.contains("params") ? ObjectOrEmpty(message["params"]) : kEmptyObject;
        if (!params.contains("name") || !params["name"].is_string()) {
            return mcp::RpcError(id, -32602, "Missing tool name");
        }
        const std::string toolName = params["name"].get<std::string>();
        if (!CommandRegistry::self().has(toolName)) {
            return mcp::RpcError(id, -32602, "Unknown tool: " + toolName);
        }
        return mcp::RpcResult(id, dispatchToolCall(params));
    }
    if (method == "resources/list" || method == "resources/read") {
        nlohmann::json args = nlohmann::json::object();
        if (method == "resources/read") {
            const nlohmann::json& params =
                message.contains("params") ? ObjectOrEmpty(message["params"]) : kEmptyObject;
            args["uri"] = params.value("uri", std::string());
        }
        nlohmann::json result = dispatchInternal(
            method == "resources/list" ? "@resources/list" : "@resources/read", args);
        if (result.is_object() && result.contains("error")) {
            return mcp::RpcError(id, -32602, result.value("error", std::string()));
        }
        if (method == "resources/read") {
            return mcp::RpcResult(id, nlohmann::json{{"contents", nlohmann::json::array(
                {nlohmann::json{{"uri", args.value("uri", std::string())},
                                {"mimeType", "application/json"},
                                {"text", result.dump()}}})}});
        }
        return mcp::RpcResult(id, std::move(result));
    }
    if (method == "prompts/list") {
        return mcp::RpcResult(id, BuildPromptList());
    }
    if (method == "prompts/get") {
        const nlohmann::json& params =
            message.contains("params") ? ObjectOrEmpty(message["params"]) : kEmptyObject;
        const std::string name = params.value("name", std::string());
        const nlohmann::json& arguments =
            params.contains("arguments") ? ObjectOrEmpty(params["arguments"]) : kEmptyObject;
        auto result = BuildPromptResult(name, arguments);
        if (!result) {
            return mcp::RpcError(id, -32602, "Unknown prompt: " + name);
        }
        return mcp::RpcResult(id, std::move(*result));
    }
    if (hasId) {
        return mcp::RpcError(id, -32601, "Method not found: " + method);
    }
    return std::nullopt;
}

std::shared_ptr<McpToolCall> McpServer::enqueueToolCall(const nlohmann::json& params) {
    auto call = std::make_shared<McpToolCall>();
    call->name = params["name"].get<std::string>();
    call->args = params.contains("arguments") ? ObjectOrEmpty(params["arguments"])
                                              : kEmptyObject;
    if (params.contains("_meta") && params["_meta"].is_object()) {
        const auto it = params["_meta"].find("progressToken");
        if (it != params["_meta"].end() && !it->is_null()) {
            call->progressToken = *it;
        }
    }
    std::lock_guard<std::mutex> lock(m_callMutex);
    m_pending.push_back(call);
    return call;
}

nlohmann::json McpServer::awaitToolCall(const std::shared_ptr<McpToolCall>& call) {
    std::unique_lock<std::mutex> lock(m_callMutex);
    const bool finished =
        m_callCv.wait_for(lock, kToolCallTimeout, [&] { return call->completed; });
    if (!finished) {
        call->cancelled = true;
        return mcp::ToolCallResult(
            nlohmann::json{{"ok", false}, {"error", "Tool call timed out"}}.dump(), true);
    }
    return mcp::ToolCallResult(call->result.dump(), !call->ok);
}

nlohmann::json McpServer::dispatchToolCall(const nlohmann::json& params) {
    auto call = enqueueToolCall(params);
    {
        std::lock_guard<std::mutex> lock(m_callMutex);
        if (!m_running.load(std::memory_order_acquire)) {
            call->ok = false;
            call->result = nlohmann::json{{"ok", false}, {"error", "MCP server is stopping"}};
            call->completed = true;
        }
    }
    m_callCv.notify_all();
    if (call->completed) {
        return mcp::ToolCallResult(call->result.dump(), true);
    }
    return awaitToolCall(call);
}

nlohmann::json McpServer::dispatchInternal(const std::string& name, const nlohmann::json& args) {
    auto call = std::make_shared<McpToolCall>();
    call->name = name;
    call->args = args;
    {
        std::lock_guard<std::mutex> lock(m_callMutex);
        if (!m_running.load(std::memory_order_acquire)) {
            call->ok = false;
            call->result = nlohmann::json{{"error", "MCP server is stopping"}};
            call->completed = true;
        } else {
            m_pending.push_back(call);
        }
    }
    m_callCv.notify_all();
    std::unique_lock<std::mutex> lock(m_callMutex);
    m_callCv.wait(lock, [&] { return call->completed; });
    return call->result;
}

void McpServer::startBenchmarkCall(const std::shared_ptr<McpToolCall>& call) {
    if (!m_session || !AICommandBridge::self().isAllowed(call->name)) {
        finishCall(call, false,
                   nlohmann::json{{"ok", false}, {"error", "Command not allowed: " + call->name}});
        return;
    }
    int frames = 240;
    if (auto it = call->args.find("frames");
        it != call->args.end() && it->is_number_integer()) {
        frames = it->get<int>();
    }
    if (!call->progressToken.is_null()) {
        pushCallEvent(call, ProgressNotification(call->progressToken, 0, std::clamp(frames, 16, 3600)));
    }
    auto progress = [this, call](int done, int total) {
        if (!call->progressToken.is_null()) {
            pushCallEvent(call, ProgressNotification(call->progressToken, done, total));
        }
    };
    auto finished = [this, call](bool ok, nlohmann::json result) {
        finishCall(call, ok, std::move(result));
    };
    if (!m_session->startBenchmark(frames, std::move(finished), std::move(progress))) {
        finishCall(call, false,
                   nlohmann::json{{"ok", false},
                                  {"error", "Benchmark unavailable (runtime not booted or busy)"}});
    }
}

void McpServer::drain() {
    if (!m_running.load(std::memory_order_acquire)) {
        return;
    }
    std::deque<std::shared_ptr<McpToolCall>> batch;
    {
        std::lock_guard<std::mutex> lock(m_callMutex);
        batch.swap(m_pending);
    }
    if (batch.empty()) {
        return;
    }
    for (auto& call : batch) {
        if (call->completed) {
            continue;
        }
        if (call->cancelled) {
            std::lock_guard<std::mutex> lock(m_callMutex);
            call->ok = false;
            call->result = nlohmann::json{{"ok", false}, {"error", "Tool call cancelled"}};
            call->completed = true;
            continue;
        }
        if (call->name == "benchmark.run") {
            startBenchmarkCall(call);
            continue;
        }
        if (call->name.rfind("@", 0) == 0) {
            EditorSession* session = m_session;
            nlohmann::json out = nlohmann::json::object();
            if (!session) {
                out = nlohmann::json{{"error", "No editor session"}};
            } else if (call->name == "@resources/list") {
                WorkflowLibrary::self().setStoragePath(WorkflowStoragePath(*session));
                nlohmann::json items = nlohmann::json::array();
                items.push_back(nlohmann::json{
                    {"uri", "cakery://scene/current"},
                    {"name", "Current scene document"},
                    {"mimeType", "application/json"}});
                items.push_back(nlohmann::json{
                    {"uri", "cakery://recorder/records"},
                    {"name", "Execution recorder history"},
                    {"mimeType", "application/json"}});
                for (const std::string& name : WorkflowLibrary::self().names()) {
                    items.push_back(nlohmann::json{
                        {"uri", "cakery://workflow-template/" + name},
                        {"name", "Workflow template: " + name},
                        {"mimeType", "application/json"}});
                }
                out = nlohmann::json{{"resources", std::move(items)}};
            } else if (call->name == "@resources/read") {
                const std::string uri = call->args.value("uri", std::string());
                nlohmann::json text = nullptr;
                if (uri == "cakery://scene/current") {
                    if (session->documentModel().hasDocument()) {
                        text = EditorDocumentSerializer::toJson(session->documentModel().document());
                    }
                } else if (uri == "cakery://recorder/records") {
                    text = ExecutionRecorder::self().toJSON(256, false, "all");
                } else if (uri.rfind("cakery://workflow-template/", 0) == 0) {
                    WorkflowLibrary::self().setStoragePath(WorkflowStoragePath(*session));
                    static constexpr const char* kTemplatePrefix = "cakery://workflow-template/";
                    const std::string name = uri.substr(std::string(kTemplatePrefix).size());
                    const auto* steps = WorkflowLibrary::self().find(name);
                    if (steps) {
                        nlohmann::json data;
                        data["name"] = name;
                        data["steps"] = nlohmann::json::array();
                        for (const WorkflowStep& step : *steps) {
                            data["steps"].push_back(nlohmann::json{
                                {"command", step.command}, {"args", step.args}});
                        }
                        text = std::move(data);
                    }
                }
                out = text.is_null() ? nlohmann::json{{"error", "Unknown resource: " + uri}}
                                     : std::move(text);
            } else {
                out = nlohmann::json{{"error", "Unknown internal request: " + call->name}};
            }
            const bool ok = !out.contains("error");
            {
                std::lock_guard<std::mutex> lock(m_callMutex);
                call->ok = ok;
                call->result = std::move(out);
                call->completed = true;
            }
            continue;
        }
        nlohmann::json out = AICommandBridge::self().execute(call->name, call->args);
        {
            std::lock_guard<std::mutex> lock(m_callMutex);
            call->ok = out.value("ok", false);
            call->result = std::move(out);
            call->completed = true;
        }
    }
    m_callCv.notify_all();
}

#else // _WIN32

McpServer& McpServer::self() {
    static McpServer instance;
    return instance;
}

McpServer::~McpServer() = default;

bool McpServer::start(EditorSession&, std::uint16_t) {
    return false;
}

void McpServer::stop() {
    m_running.store(false, std::memory_order_release);
}

std::size_t McpServer::pendingCount() const {
    return 0;
}

std::string McpServer::currentSessionId() const {
    return {};
}

void McpServer::finishCall(const std::shared_ptr<McpToolCall>&, bool, nlohmann::json) {}

void McpServer::pushCallEvent(const std::shared_ptr<McpToolCall>&, nlohmann::json) {}

std::optional<nlohmann::json> McpServer::handleRpc(const nlohmann::json&) {
    return std::nullopt;
}

std::shared_ptr<McpToolCall> McpServer::enqueueToolCall(const nlohmann::json&) {
    return nullptr;
}

nlohmann::json McpServer::awaitToolCall(const std::shared_ptr<McpToolCall>&) {
    return {};
}

nlohmann::json McpServer::dispatchToolCall(const nlohmann::json&) {
    return {};
}

nlohmann::json McpServer::dispatchInternal(const std::string&, const nlohmann::json&) {
    return {};
}

void McpServer::startBenchmarkCall(const std::shared_ptr<McpToolCall>&) {}

void McpServer::drain() {}

std::string McpServer::processRpcPayload(const nlohmann::json&) {
    return {};
}

#endif // _WIN32

} // namespace cakery
