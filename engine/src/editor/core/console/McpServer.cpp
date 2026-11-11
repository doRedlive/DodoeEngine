// do@Redlive

#include "dopch.h"

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

String HttpResponse(int code, const char* reason, const String& body,
                         const String& extraHeaders = {}) {
    String head = "HTTP/1.1 ";
    head += ToString(code);
    head += " ";
    head += reason;
    head += "\r\nContent-Type: application/json\r\nContent-Length: ";
    head += ToString(body.size());
    head += "\r\nConnection: close";
    if (!extraHeaders.empty()) {
        head += "\r\n";
        head += extraHeaders;
    }
    head += "\r\n\r\n";
    return head + body;
}

bool SendAll(SOCKET socket, const String& payload) {
    Size_t sent = 0;
    while (sent < payload.size()) {
        const int result = send(socket, payload.data() + sent,
                                static_cast<int>(payload.size() - sent), 0);
        if (result <= 0) {
            return false;
        }
        sent += static_cast<Size_t>(result);
    }
    return true;
}

Json NotificationMessage(const String& level, const String& data) {
    return Json{{"jsonrpc", "2.0"},
                          {"method", "notifications/message"},
                          {"params", Json{{"level", level}, {"data", data}}}};
}

Json ProgressNotification(const Json& token, int done, int total) {
    Json params;
    params["progressToken"] = token;
    params["progress"] = done;
    params["total"] = total;
    params["message"] = "Benchmark frames: " + ToString(done) + "/" + ToString(total);
    return Json{{"jsonrpc", "2.0"},
                          {"method", "notifications/progress"},
                          {"params", std::move(params)}};
}

bool PayloadRequestsInitialize(const Json& payload) {
    if (payload.is_array()) {
        for (const auto& item : payload) {
            if (PayloadRequestsInitialize(item)) {
                return true;
            }
        }
        return false;
    }
    return payload.is_object() && payload.contains("method") &&
           payload["method"].is_string() && payload["method"].get<String>() == "initialize";
}

const Json kEmptyObject = Json::object();

const Json& ObjectOrEmpty(const Json& value) {
    return value.is_object() ? value : kEmptyObject;
}

Json BuildPromptList() {
    auto argument = [](const String& name, const String& description, bool required) {
        Json item;
        item["name"] = name;
        item["description"] = description;
        item["required"] = required;
        return item;
    };
    auto prompt = [](const String& name, const String& description,
                     Json arguments) {
        Json item;
        item["name"] = name;
        item["description"] = description;
        item["arguments"] = std::move(arguments);
        return item;
    };
    Json optimizeArgs = Json::array();
    optimizeArgs.push_back(argument("focus", "draw calls, overdraw, geometry or textures", false));
    Json inspectArgs = Json::array();
    inspectArgs.push_back(argument("uuid", "Entity UUID", true));

    Json prompts = Json::array();
    prompts.push_back(prompt("optimize_scene",
                             "Analyze the active scene and propose performance improvements",
                             std::move(optimizeArgs)));
    prompts.push_back(prompt("summarize_workflow",
                             "Summarize the recorded execution workflow graph",
                             Json::array()));
    prompts.push_back(prompt("inspect_entity",
                             "Inspect one entity: components, field values and dependencies",
                             std::move(inspectArgs)));
    Json out;
    out["prompts"] = std::move(prompts);
    return out;
}

Optional<Json> BuildPromptResult(const String& name,
                                                const Json& arguments) {
    String description;
    String text;
    if (name == "optimize_scene") {
        const String focus = arguments.value("focus", String("draw calls"));
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
        const String uuid = arguments.value("uuid", String());
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
    Json content;
    content["type"] = "text";
    content["text"] = text;
    Json message;
    message["role"] = "user";
    message["content"] = std::move(content);
    Json messages = Json::array();
    messages.push_back(std::move(message));
    Json result;
    result["description"] = description;
    result["messages"] = std::move(messages);
    return result;
}

#endif // _WIN32

} // namespace

#ifdef _WIN32

McpServer::McpServer() = default;

McpServer::~McpServer() {
    stop();
}

bool McpServer::start(EditorSession& session, UInt16 port) {
    (void)session;
    (void)port;
    return false;
}

void McpServer::stop() {
    m_running.store(false, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(m_callMutex);
        for (auto& call : m_pending) {
            call->ok = false;
            call->result = Json{{"ok", false}, {"error", "MCP server is stopping"}};
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
    std::vector<std::thread> clients;
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        for (const auto handle : m_clientSockets) {
            shutdown(static_cast<SOCKET>(handle), SD_BOTH);
        }
        clients = std::move(m_clientThreads);
        m_clientThreads.clear();
        m_clientSockets.clear();
    }
    for (auto& client : clients) {
        if (client.joinable()) {
            client.join();
        }
    }
    if (m_session) {
        m_session->aiBridge().detach();
    }
    m_session = nullptr;
    m_port = 0;
}

Size_t McpServer::pendingCount() const {
    std::lock_guard<std::mutex> lock(m_callMutex);
    return m_pending.size();
}

String McpServer::currentSessionId() const {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_mcpSessionId;
}

void McpServer::finishCall(const Ref<McpToolCall>& call, bool ok, Json result) {
    {
        std::lock_guard<std::mutex> lock(m_callMutex);
        call->ok = ok;
        call->result = std::move(result);
        call->completed = true;
    }
    m_callCv.notify_all();
}

void McpServer::pushCallEvent(const Ref<McpToolCall>& call, Json event) {
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
        {
            std::lock_guard<std::mutex> lock(m_clientsMutex);
            m_clientSockets.push_back(static_cast<std::uintptr_t>(client));
            m_clientThreads.emplace_back(&McpServer::serveClient, this,
                                         static_cast<std::uintptr_t>(client));
            pruneFinishedClients();
        }
    }
}

void McpServer::pruneFinishedClients() {
    for (auto it = m_clientThreads.begin(); it != m_clientThreads.end();) {
        if (WaitForSingleObject(it->native_handle(), 0) == WAIT_OBJECT_0) {
            it->join();
            it = m_clientThreads.erase(it);
        } else {
            ++it;
        }
    }
}

void McpServer::serveClient(std::uintptr_t socketHandle) {
    const SOCKET client = static_cast<SOCKET>(socketHandle);
    DWORD timeout = kSocketTimeoutMs;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout),
               sizeof(timeout));
    setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout),
               sizeof(timeout));

    auto feed = [&client](mcp::HttpBuffer& buffer) {
        char chunk[8192];
        const int received = recv(client, chunk, static_cast<int>(sizeof(chunk)), 0);
        if (received <= 0) {
            return false;
        }
        buffer.feed(chunk, static_cast<Size_t>(received));
        return true;
    };

    String response;
    String payload;
    String extraHeaders;
    bool streamResponse = false;
    Json message = nullptr;
    mcp::HttpRequestHead head;
    mcp::HttpBuffer buffer;
    String body;
    const bool pathOk = mcp::ParseRequestHead(buffer, head, feed) && head.valid &&
                        head.target == kMcpEndpoint;
    if (!pathOk) {
        response = HttpResponse(404, "Not Found", "{\"error\":\"not found\"}");
    } else if (head.method != "POST") {
        response = HttpResponse(405, "Method Not Allowed", "{\"error\":\"POST only\"}");
    } else if (!mcp::ReadBody(buffer, head.content_length, body, feed)) {
        response = HttpResponse(400, "Bad Request", "{\"error\":\"incomplete body\"}");
    } else {
        message = Json::parse(body, nullptr, false);
    }

    if (response.empty() && (message.is_discarded() || message.is_null())) {
        payload = ToString(mcp::RpcError(Json(nullptr), -32700, "Parse error"));
    } else if (response.empty()) {
        const String assigned = currentSessionId();
        if (!assigned.empty() && !PayloadRequestsInitialize(message) &&
            head.session_id != assigned) {
            response = HttpResponse(404, "Not Found", "{\"error\":\"session not found\"}");
        }
    }

    if (response.empty() && !message.is_null() && !message.is_discarded()) {
        const String method =
            message.is_object() && message.contains("method") && message["method"].is_string()
                ? message["method"].get<String>()
                : String();
        const String sessionIdBefore = currentSessionId();
        if (method == "tools/call") {
            streamResponse = true;
        } else {
            payload = processRpcPayload(message);
        }
        const String sessionIdAfter = currentSessionId();
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
        const Json& params =
            message.is_object() && message.contains("params")
                ? ObjectOrEmpty(message["params"])
                : kEmptyObject;
        if (!m_session || !params.contains("name") || !params["name"].is_string() ||
            !m_session->commands().has(params["name"].get<String>())) {
            const char* reason = params.contains("name") ? "Unknown tool" : "Missing tool name";
            const Json id =
                message.contains("id") ? message["id"] : Json(nullptr);
            SendAll(client, String("event: message\ndata: ") +
                                ToString(mcp::RpcError(id, -32602, reason)) + "\n\n");
        } else {
            auto call = enqueueToolCall(params);
            const auto deadline = std::chrono::steady_clock::now() + kToolCallTimeout;
            pushCallEvent(call, NotificationMessage("info", "Executing " + call->name));
            while (true) {
                DynamicArray<Json> events;
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
                    SendAll(client, String("event: message\ndata: ") + ToString(event) + "\n\n");
                }
                if (completed) {
                    break;
                }
                if (!m_running.load(std::memory_order_acquire) ||
                    std::chrono::steady_clock::now() > deadline) {
                    finishCall(call, false,
                               Json{{"ok", false},
                                              {"error", "Tool call timed out or server stopping"}});
                }
            }
            const Json result = awaitToolCall(call);
            const Json id = message.contains("id") ? message["id"] : Json(nullptr);
            SendAll(client,
                    String("event: message\ndata: ") + ToString(mcp::RpcResult(id, result)) + "\n\n");
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        for (auto it = m_clientSockets.begin(); it != m_clientSockets.end(); ++it) {
            if (*it == socketHandle) {
                m_clientSockets.erase(it);
                break;
            }
        }
    }
    shutdown(client, SD_BOTH);
    closesocket(client);
}

String McpServer::processRpcPayload(const Json& payload) {
    if (payload.is_array()) {
        Json responses = Json::array();
        for (const auto& item : payload) {
            if (auto response = handleRpc(item)) {
                responses.push_back(std::move(*response));
            }
        }
        return responses.empty() ? String() : ToString(responses);
    }
    if (auto response = handleRpc(payload)) {
        return ToString(*response);
    }
    return String();
}

Optional<Json> McpServer::handleRpc(const Json& message) {
    if (!message.is_object() || !message.contains("method") || !message["method"].is_string()) {
        const Json id =
            message.is_object() && message.contains("id") ? message["id"] : Json(nullptr);
        return mcp::RpcError(id, -32600, "Invalid Request");
    }
    const String method = message["method"].get<String>();
    const bool hasId = message.contains("id");
    const Json id = hasId ? message["id"] : Json(nullptr);

    if (method.rfind("notifications/", 0) == 0) {
        return std::nullopt;
    }
    if (method == "initialize") {
        String requested;
        if (message.contains("params") && message["params"].is_object()) {
            const auto it = message["params"].find("protocolVersion");
            if (it != message["params"].end() && it->is_string()) {
                requested = it->get<String>();
            }
        }
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (m_mcpSessionId.empty()) {
            m_mcpSessionId = ToString(
                std::chrono::steady_clock::now().time_since_epoch().count());
        }
        return mcp::RpcResult(id, mcp::BuildInitializeResult(requested));
    }
    if (method == "ping") {
        return mcp::RpcResult(id, Json::object());
    }
    if (method == "tools/list") {
        return mcp::RpcResult(id, mcp::BuildToolCatalog(
            m_session ? m_session->commands().toolSchema() : Json::array()));
    }
    if (method == "tools/call") {
        const Json& params =
            message.contains("params") ? ObjectOrEmpty(message["params"]) : kEmptyObject;
        if (!params.contains("name") || !params["name"].is_string()) {
            return mcp::RpcError(id, -32602, "Missing tool name");
        }
        const String toolName = params["name"].get<String>();
        if (!m_session || !m_session->commands().has(toolName)) {
            return mcp::RpcError(id, -32602, "Unknown tool: " + toolName);
        }
        return mcp::RpcResult(id, dispatchToolCall(params));
    }
    if (method == "resources/list" || method == "resources/read") {
        Json args = Json::object();
        if (method == "resources/read") {
            const Json& params =
                message.contains("params") ? ObjectOrEmpty(message["params"]) : kEmptyObject;
            args["uri"] = params.value("uri", String());
        }
        Json result = dispatchInternal(
            method == "resources/list" ? "@resources/list" : "@resources/read", args);
        if (result.is_object() && result.contains("error")) {
            return mcp::RpcError(id, -32602, result.value("error", String()));
        }
        if (method == "resources/read") {
            return mcp::RpcResult(id, Json{{"contents", Json::array(
                {Json{{"uri", args.value("uri", String())},
                                {"mimeType", "application/json"},
                                {"text", result.dump()}}})}});
        }
        return mcp::RpcResult(id, std::move(result));
    }
    if (method == "prompts/list") {
        return mcp::RpcResult(id, BuildPromptList());
    }
    if (method == "prompts/get") {
        const Json& params =
            message.contains("params") ? ObjectOrEmpty(message["params"]) : kEmptyObject;
        const String name = params.value("name", String());
        const Json& arguments =
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

Ref<McpToolCall> McpServer::enqueueToolCall(const Json& params) {
    auto call = create_ref<McpToolCall>();
    call->name = params["name"].get<String>();
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

Json McpServer::awaitToolCall(const Ref<McpToolCall>& call) {
    std::unique_lock<std::mutex> lock(m_callMutex);
    const bool finished =
        m_callCv.wait_for(lock, kToolCallTimeout, [&] { return call->completed; });
    if (!finished) {
        call->cancelled = true;
        return mcp::ToolCallResult(
            ToString(Json{{"ok", false}, {"error", "Tool call timed out"}}), true);
    }
    return mcp::ToolCallResult(ToString(call->result), !call->ok);
}

Json McpServer::dispatchToolCall(const Json& params) {
    auto call = enqueueToolCall(params);
    {
        std::lock_guard<std::mutex> lock(m_callMutex);
        if (!m_running.load(std::memory_order_acquire)) {
            call->ok = false;
            call->result = Json{{"ok", false}, {"error", "MCP server is stopping"}};
            call->completed = true;
        }
    }
    m_callCv.notify_all();
    if (call->completed) {
        return mcp::ToolCallResult(ToString(call->result), true);
    }
    return awaitToolCall(call);
}

Json McpServer::dispatchInternal(const String& name, const Json& args) {
    auto call = create_ref<McpToolCall>();
    call->name = name;
    call->args = args;
    {
        std::lock_guard<std::mutex> lock(m_callMutex);
        if (!m_running.load(std::memory_order_acquire)) {
            call->ok = false;
            call->result = Json{{"error", "MCP server is stopping"}};
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

void McpServer::startBenchmarkCall(const Ref<McpToolCall>& call) {
    if (!m_session || !m_session->aiBridge().isAllowed(call->name)) {
        finishCall(call, false,
                   Json{{"ok", false}, {"error", "Command not allowed: " + call->name}});
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
    auto finished = [this, call](bool ok, Json result) {
        finishCall(call, ok, std::move(result));
    };
    if (!m_session->startBenchmark(frames, std::move(finished), std::move(progress))) {
        finishCall(call, false,
                   Json{{"ok", false},
                                  {"error", "Benchmark unavailable (runtime not booted or busy)"}});
    }
}

void McpServer::drain() {
    if (!m_running.load(std::memory_order_acquire)) {
        return;
    }
    Deque<Ref<McpToolCall>> batch;
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
            call->result = Json{{"ok", false}, {"error", "Tool call cancelled"}};
            call->completed = true;
            continue;
        }
        if (call->name == "benchmark.run") {
            startBenchmarkCall(call);
            continue;
        }
        if (call->name.rfind("@", 0) == 0) {
            EditorSession* session = m_session;
            Json out = Json::object();
            if (!session) {
                out = Json{{"error", "No editor session"}};
            } else if (call->name == "@resources/list") {
                session->workflows().setStoragePath(WorkflowStoragePath(*session));
                Json items = Json::array();
                items.push_back(Json{
                    {"uri", "cakery://scene/current"},
                    {"name", "Current scene document"},
                    {"mimeType", "application/json"}});
                items.push_back(Json{
                    {"uri", "cakery://recorder/records"},
                    {"name", "Execution recorder history"},
                    {"mimeType", "application/json"}});
                for (const String& name : session->workflows().names()) {
                    items.push_back(Json{
                        {"uri", "cakery://workflow-template/" + name},
                        {"name", "Workflow template: " + name},
                        {"mimeType", "application/json"}});
                }
                out = Json{{"resources", std::move(items)}};
            } else if (call->name == "@resources/read") {
                const String uri = call->args.value("uri", String());
                Json text = nullptr;
                if (uri == "cakery://scene/current") {
                    if (session->documentModel().hasDocument()) {
                        text = EditorDocumentSerializer::toJson(session->documentModel().document());
                    }
                } else if (uri == "cakery://recorder/records") {
                    if (m_session) {
                        text = m_session->executionRecorder().toJSON(256, false, "all");
                    }
                } else if (uri.rfind("cakery://workflow-template/", 0) == 0) {
                    session->workflows().setStoragePath(WorkflowStoragePath(*session));
                    static constexpr const char* kTemplatePrefix = "cakery://workflow-template/";
                    const String name = uri.substr(String(kTemplatePrefix).size());
                    const auto* steps = session->workflows().find(name);
                    if (steps) {
                        Json data;
                        data["name"] = name;
                        data["steps"] = Json::array();
                        for (const WorkflowStep& step : *steps) {
                            data["steps"].push_back(Json{
                                {"command", step.command}, {"args", step.args}});
                        }
                        text = std::move(data);
                    }
                }
                out = text.is_null() ? Json{{"error", "Unknown resource: " + uri}}
                                     : std::move(text);
            } else {
                out = Json{{"error", "Unknown internal request: " + call->name}};
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
        Json out = m_session
            ? m_session->aiBridge().execute(call->name, call->args)
            : Json{{"ok", false}, {"error", "No editor session"}};
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

McpServer::McpServer() = default;

McpServer::~McpServer() = default;

bool McpServer::start(EditorSession&, UInt16) {
    return false;
}

void McpServer::stop() {
    m_running.store(false, std::memory_order_release);
}

Size_t McpServer::pendingCount() const {
    return 0;
}

String McpServer::currentSessionId() const {
    return {};
}

void McpServer::finishCall(const Ref<McpToolCall>&, bool, Json) {}

void McpServer::pushCallEvent(const Ref<McpToolCall>&, Json) {}

Optional<Json> McpServer::handleRpc(const Json&) {
    return std::nullopt;
}

Ref<McpToolCall> McpServer::enqueueToolCall(const Json&) {
    return nullptr;
}

Json McpServer::awaitToolCall(const Ref<McpToolCall>&) {
    return {};
}

Json McpServer::dispatchToolCall(const Json&) {
    return {};
}

Json McpServer::dispatchInternal(const String&, const Json&) {
    return {};
}

void McpServer::startBenchmarkCall(const Ref<McpToolCall>&) {}

void McpServer::drain() {}

String McpServer::processRpcPayload(const Json&) {
    return {};
}

#endif // _WIN32

} // namespace cakery
