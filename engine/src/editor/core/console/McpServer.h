// do@Redlive
#pragma once
#include "dopch.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace cakery {

class EditorSession;

inline constexpr UInt16 kDefaultMcpPort = 7800;

struct McpToolCall {
    String name;
    Json args;
    Json progressToken;
    Json result;
    bool ok = false;
    bool completed = false;
    bool cancelled = false;
    DynamicArray<Json> events;
};

class McpServer {
public:
    McpServer();
    ~McpServer();

    McpServer(const McpServer&) = delete;
    McpServer& operator=(const McpServer&) = delete;

    bool start(EditorSession& session, UInt16 port);
    void stop();

    bool isRunning() const { return m_running.load(std::memory_order_acquire); }
    UInt16 port() const { return m_port; }
    Size_t pendingCount() const;

    void drain();

    void finishCall(const Ref<McpToolCall>& call, bool ok, Json result);
    void pushCallEvent(const Ref<McpToolCall>& call, Json event);

private:
    void acceptLoop();
    void serveClient(std::uintptr_t socketHandle);
    void pruneFinishedClients();
    Optional<Json> handleRpc(const Json& message);
    Ref<McpToolCall> enqueueToolCall(const Json& params);
    Json awaitToolCall(const Ref<McpToolCall>& call);
    Json dispatchToolCall(const Json& params);
    Json dispatchInternal(const String& name, const Json& args);
    void startBenchmarkCall(const Ref<McpToolCall>& call);
    String processRpcPayload(const Json& payload);
    String currentSessionId() const;

    EditorSession* m_session = nullptr;
    std::thread m_acceptThread;
    std::uintptr_t m_listenSocket = 0;
    UInt16 m_port = 0;
    std::atomic<bool> m_running{false};

    std::mutex m_clientsMutex;
    Deque<std::uintptr_t> m_clientSockets;
    std::vector<std::thread> m_clientThreads;

    mutable std::mutex m_stateMutex;
    String m_mcpSessionId;

    mutable std::mutex m_callMutex;
    std::condition_variable m_callCv;
    Deque<Ref<McpToolCall>> m_pending;
};

} // namespace cakery
