// do@Redlive

#include "AICommandBridge.h"

#include "CommandRegistry.h"
#include "ExecutionRecorder.h"
#include "core/history/EditHistory.h"
#include "core/EditorSession.h"

#include <algorithm>

namespace cakery {

AICommandBridge& AICommandBridge::self() {
    static AICommandBridge instance;
    return instance;
}

void AICommandBridge::attach(EditorSession& session) {
    m_session = &session;
}

void AICommandBridge::detach() {
    if (m_inSession) {
        endSession(true);
    }
    m_session = nullptr;
}

std::string AICommandBridge::beginSession(const std::string& goal) {
    if (!m_session) {
        return {};
    }
    if (m_inSession) {
        endSession(true);
    }
    m_inSession = true;
    m_sessionGoal = goal;
    m_session->editHistory().beginTransaction(Author::AI, goal);
    m_sessionCheckpoint = m_session->editHistory().createCheckpoint(goal, Author::AI, CheckpointMode::DocumentSnapshot);
    m_sessionId = std::to_string(m_sessionCheckpoint);
    ExecutionRecord marker;
    marker.command = "session.begin";
    marker.data = nlohmann::json{{"goal", goal}, {"checkpoint", m_sessionCheckpoint}};
    marker.author = "ai";
    marker.session = m_sessionId;
    marker.ok = true;
    ExecutionRecorder::self().record(std::move(marker));
    return m_sessionId;
}

void AICommandBridge::endSession(bool keep) {
    if (!m_inSession || !m_session) return;
    m_inSession = false;
    ExecutionRecord marker;
    marker.command = "session.end";
    marker.data = nlohmann::json{{"kept", keep}};
    marker.author = "ai";
    marker.session = m_sessionId;
    marker.ok = true;
    ExecutionRecorder::self().record(std::move(marker));
    m_sessionId.clear();
    m_sessionGoal.clear();
    if (keep) {
        m_session->editHistory().commit();
    } else {
        m_session->editHistory().revertTo(m_sessionCheckpoint);
    }
}

nlohmann::json AICommandBridge::listCommands() {
    return CommandRegistry::self().toolSchema();
}

nlohmann::json AICommandBridge::execute(const std::string& name, const nlohmann::json& args) {
    if (!m_session) {
        nlohmann::json err;
        err["ok"] = false;
        err["error"] = "No editor session attached";
        return err;
    }
    if (!isAllowed(name)) {
        nlohmann::json err;
        err["ok"] = false;
        err["error"] = "Command not allowed: " + name;
        return err;
    }
    CommandContext context;
    context.author = "ai";
    context.session = m_sessionId;
    auto result = CommandRegistry::self().executeStructured(*m_session, name, args, context);
    nlohmann::json out;
    out["ok"] = result.ok;
    out["message"] = result.message;
    out["data"] = result.data;
    return out;
}

nlohmann::json AICommandBridge::query(const std::string& name, const nlohmann::json& args) {
    if (!m_session) {
        nlohmann::json err;
        err["ok"] = false;
        err["error"] = "No editor session attached";
        return err;
    }
    CommandContext context;
    context.author = "ai";
    context.session = m_sessionId;
    auto result = CommandRegistry::self().executeStructured(*m_session, name, args, context);
    nlohmann::json out;
    out["ok"] = result.ok;
    out["message"] = result.message;
    out["data"] = result.data;
    return out;
}

bool AICommandBridge::isDenied(const std::string& name) const {
    for (auto& d : m_denylist) {
        if (name == d || name.rfind(d + ".", 0) == 0) return true;
    }
    return false;
}

bool AICommandBridge::isAllowed(const std::string& name) const {
    if (isDenied(name)) return false;
    if (m_allowlist.empty()) return true;
    for (auto& a : m_allowlist) {
        if (name == a || name.rfind(a + ".", 0) == 0) return true;
    }
    return false;
}

} // namespace cakery
