// do@Redlive

#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace cakery {

class EditorSession;

class AICommandBridge {
public:
    static AICommandBridge& self();

    AICommandBridge(const AICommandBridge&) = delete;
    AICommandBridge& operator=(const AICommandBridge&) = delete;

    void attach(EditorSession& session);
    void detach();
    EditorSession* session() const { return m_session; }

    std::string beginSession(const std::string& goal);
    void endSession(bool keep);
    bool inSession() const { return m_inSession; }
    std::string sessionId() const { return m_sessionId; }

    nlohmann::json listCommands();
    nlohmann::json execute(const std::string& name, const nlohmann::json& args);
    nlohmann::json query(const std::string& name, const nlohmann::json& args);

    void setAllowlist(std::vector<std::string> names) { m_allowlist = std::move(names); }
    void setDenylist(std::vector<std::string> names) { m_denylist = std::move(names); }
    const std::vector<std::string>& allowlist() const { return m_allowlist; }
    const std::vector<std::string>& denylist() const { return m_denylist; }
    bool isAllowed(const std::string& name) const;

private:
    AICommandBridge() = default;

    bool isDenied(const std::string& name) const;

    EditorSession* m_session = nullptr;
    bool m_inSession = false;
    uint64_t m_sessionCheckpoint = 0;
    std::string m_sessionId;
    std::string m_sessionGoal;
    std::vector<std::string> m_allowlist;
    std::vector<std::string> m_denylist;
};

} // namespace cakery
