// do@Redlive

#include "AICommandBridge.h"

#include "core/console/CommandRegistry.h"
#include "core/history/EditHistory.h"
#include "core/EditorSession.h"

namespace cakery {

std::string AICommandBridge::beginSession(const std::string& goal) {
    if (m_inSession) {
        endSession(true);
    }
    m_inSession = true;
    m_session.editHistory().beginTransaction(Author::AI, goal);
    m_sessionCheckpoint = m_session.editHistory().createCheckpoint(goal, Author::AI, CheckpointMode::DocumentSnapshot);
    return std::to_string(m_sessionCheckpoint);
}

void AICommandBridge::endSession(bool keep) {
    if (!m_inSession) return;
    m_inSession = false;
    if (keep) {
        m_session.editHistory().commit();
    } else {
        m_session.editHistory().revertTo(m_sessionCheckpoint);
    }
}

nlohmann::json AICommandBridge::listCommands() {
    return CommandRegistry::self().toolSchema();
}

nlohmann::json AICommandBridge::execute(const std::string& name, const nlohmann::json& args) {
    if (!isAllowed(name)) {
        nlohmann::json err;
        err["ok"] = false;
        err["error"] = "Command not allowed: " + name;
        return err;
    }
    auto result = CommandRegistry::self().executeStructured(m_session, name, args);
    nlohmann::json out;
    out["ok"] = result.ok;
    out["message"] = result.message;
    out["data"] = result.data;
    return out;
}

nlohmann::json AICommandBridge::query(const std::string& name, const nlohmann::json& args) {
    auto result = CommandRegistry::self().executeStructured(m_session, name, args);
    nlohmann::json out;
    out["ok"] = result.ok;
    out["message"] = result.message;
    out["data"] = result.data;
    return out;
}

bool AICommandBridge::isAllowed(const std::string& name) const {
    if (m_allowlist.empty()) return true;
    for (auto& a : m_allowlist) {
        if (name == a || name.rfind(a + ".", 0) == 0) return true;
    }
    return false;
}

} // namespace cakery
