// do@Redlive

#include "dopch.h"

#include "AICommandBridge.h"

#include "CommandRegistry.h"
#include "ExecutionRecorder.h"
#include "core/history/EditHistory.h"
#include "core/EditorSession.h"

#include <algorithm>

namespace cakery {

AICommandBridge::AICommandBridge() = default;

void AICommandBridge::attach(EditorSession& session) {
    m_session = &session;
}

void AICommandBridge::detach() {
    if (m_inSession) {
        endSession(true);
    }
    m_session = nullptr;
}

String AICommandBridge::beginSession(const String& goal) {
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
    m_sessionId = ToString(m_sessionCheckpoint);
    ExecutionRecord marker;
    marker.command = "session.begin";
    marker.data = Json{{"goal", goal}, {"checkpoint", m_sessionCheckpoint}};
    marker.author = "ai";
    marker.session = m_sessionId;
    marker.ok = true;
    m_session->executionRecorder().record(std::move(marker));
    return m_sessionId;
}

void AICommandBridge::endSession(bool keep) {
    if (!m_inSession || !m_session) return;
    m_inSession = false;
    ExecutionRecord marker;
    marker.command = "session.end";
    marker.data = Json{{"kept", keep}};
    marker.author = "ai";
    marker.session = m_sessionId;
    marker.ok = true;
    m_session->executionRecorder().record(std::move(marker));
    m_sessionId.clear();
    m_sessionGoal.clear();
    if (keep) {
        m_session->editHistory().commit();
    } else {
        m_session->editHistory().revertTo(m_sessionCheckpoint);
    }
}

Json AICommandBridge::listCommands() {
    if (!m_session) {
        return Json::array();
    }
    return m_session->commands().toolSchema();
}

Json AICommandBridge::execute(const String& name, const Json& args) {
    if (!m_session) {
        Json err;
        err["ok"] = false;
        err["error"] = "No editor session attached";
        return err;
    }
    if (!isAllowed(name)) {
        Json err;
        err["ok"] = false;
        err["error"] = "Command not allowed: " + name;
        return err;
    }
    CommandContext context;
    context.author = "ai";
    context.session = m_sessionId;
    auto result = m_session->commands().executeStructured(*m_session, name, args, context);
    Json out;
    out["ok"] = result.ok;
    out["message"] = result.message;
    out["data"] = result.data;
    return out;
}

Json AICommandBridge::query(const String& name, const Json& args) {
    if (!m_session) {
        Json err;
        err["ok"] = false;
        err["error"] = "No editor session attached";
        return err;
    }
    CommandContext context;
    context.author = "ai";
    context.session = m_sessionId;
    auto result = m_session->commands().executeStructured(*m_session, name, args, context);
    Json out;
    out["ok"] = result.ok;
    out["message"] = result.message;
    out["data"] = result.data;
    return out;
}

bool AICommandBridge::isDenied(const String& name) const {
    for (auto& d : m_denylist) {
        if (name == d || name.rfind(d + ".", 0) == 0) return true;
    }
    return false;
}

bool AICommandBridge::isAllowed(const String& name) const {
    if (isDenied(name)) return false;
    if (m_allowlist.empty()) return true;
    for (auto& a : m_allowlist) {
        if (name == a || name.rfind(a + ".", 0) == 0) return true;
    }
    return false;
}

} // namespace cakery
