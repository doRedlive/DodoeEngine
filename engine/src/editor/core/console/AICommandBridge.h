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
    explicit AICommandBridge(EditorSession& session) : m_session(session) {}

    std::string beginSession(const std::string& goal);
    void endSession(bool keep);

    nlohmann::json listCommands();
    nlohmann::json execute(const std::string& name, const nlohmann::json& args);
    nlohmann::json query(const std::string& name, const nlohmann::json& args);

    void setAllowlist(std::vector<std::string> names) { m_allowlist = std::move(names); }
    void setConfirmRequired(bool on) { m_confirmRequired = on; }

private:
    EditorSession& m_session;
    bool m_inSession = false;
    uint64_t m_sessionCheckpoint = 0;
    std::vector<std::string> m_allowlist;
    bool m_confirmRequired = false;

    bool isAllowed(const std::string& name) const;
};

} // namespace cakery
