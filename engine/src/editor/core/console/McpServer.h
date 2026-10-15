// do@Redlive

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace cakery {

class EditorSession;

inline constexpr std::uint16_t kDefaultMcpPort = 7800;

struct McpToolCall {
    std::string name;
    nlohmann::json args;
    nlohmann::json progressToken;
    nlohmann::json result;
    bool ok = false;
    bool completed = false;
    bool cancelled = false;
    std::vector<nlohmann::json> events;
};

class McpServer {
public:
    static McpServer& self();

    McpServer(const McpServer&) = delete;
    McpServer& operator=(const McpServer&) = delete;

    bool start(EditorSession& session, std::uint16_t port);
    void stop();

    bool isRunning() const { return m_running.load(std::memory_order_acquire); }
    std::uint16_t port() const { return m_port; }
    std::size_t pendingCount() const;

    void drain();

    void finishCall(const std::shared_ptr<McpToolCall>& call, bool ok, nlohmann::json result);
    void pushCallEvent(const std::shared_ptr<McpToolCall>& call, nlohmann::json event);

private:
    McpServer() = default;
    ~McpServer();

    void acceptLoop();
    void serveClient(std::uintptr_t socketHandle);
    std::optional<nlohmann::json> handleRpc(const nlohmann::json& message);
    std::shared_ptr<McpToolCall> enqueueToolCall(const nlohmann::json& params);
    nlohmann::json awaitToolCall(const std::shared_ptr<McpToolCall>& call);
    nlohmann::json dispatchToolCall(const nlohmann::json& params);
    nlohmann::json dispatchInternal(const std::string& name, const nlohmann::json& args);
    void startBenchmarkCall(const std::shared_ptr<McpToolCall>& call);
    std::string processRpcPayload(const nlohmann::json& payload);
    std::string currentSessionId() const;

    EditorSession* m_session = nullptr;
    std::thread m_acceptThread;
    std::uintptr_t m_listenSocket = 0;
    std::uint16_t m_port = 0;
    std::atomic<bool> m_running{false};
    std::atomic<std::size_t> m_activeConnections{0};

    mutable std::mutex m_stateMutex;
    std::string m_mcpSessionId;

    mutable std::mutex m_callMutex;
    std::condition_variable m_callCv;
    std::deque<std::shared_ptr<McpToolCall>> m_pending;
};

} // namespace cakery
