#include "AICommandBridge.h"
#include "CommandRegistry.h"
#include "framework/EditorContext.h"
#include "framework/history/EditHistory.h"

namespace cakery {

std::string AICommandBridge::beginSession(const std::string& goal) {
    if (m_inSession) {
        endSession(true);
    }
    m_inSession = true;
    m_ctx.history().beginTransaction(Author::AI, goal);
    m_sessionCheckpoint = m_ctx.history().createCheckpoint(goal, Author::AI, CheckpointMode::SceneSnapshot);
    return std::to_string(m_sessionCheckpoint);
}

void AICommandBridge::endSession(bool keep) {
    if (!m_inSession) return;
    m_inSession = false;
    if (keep) {
        m_ctx.history().commit();
    } else {
        m_ctx.history().revertTo(m_sessionCheckpoint);
    }
}

dodoe::Json AICommandBridge::listCommands() {
    return CommandRegistry::self().toolSchema();
}

dodoe::Json AICommandBridge::execute(const std::string& name, const dodoe::Json& args) {
    if (!isAllowed(name)) {
        dodoe::Json err;
        err["ok"] = false;
        err["error"] = "Command not allowed: " + name;
        return err;
    }
    auto result = CommandRegistry::self().executeStructured(m_ctx, name, args);
    dodoe::Json out;
    out["ok"] = result.ok;
    out["message"] = result.message;
    out["data"] = result.data;
    return out;
}

dodoe::Json AICommandBridge::query(const std::string& name, const dodoe::Json& args) {
    auto result = CommandRegistry::self().executeStructured(m_ctx, name, args);
    dodoe::Json out;
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
