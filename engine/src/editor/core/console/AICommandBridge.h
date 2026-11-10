// do@Redlive
#pragma once
#include "dopch.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cakery {

class EditorSession;

class AICommandBridge {
public:
    AICommandBridge();

    AICommandBridge(const AICommandBridge&) = delete;
    AICommandBridge& operator=(const AICommandBridge&) = delete;

    void attach(EditorSession& session);
    void detach();
    EditorSession* session() const { return m_session; }

    String beginSession(const String& goal);
    void endSession(bool keep);
    bool inSession() const { return m_inSession; }
    String sessionId() const { return m_sessionId; }

    Json listCommands();
    Json execute(const String& name, const Json& args);
    Json query(const String& name, const Json& args);

    void setAllowlist(DynamicArray<String> names) { m_allowlist = std::move(names); }
    void setDenylist(DynamicArray<String> names) { m_denylist = std::move(names); }
    const DynamicArray<String>& allowlist() const { return m_allowlist; }
    const DynamicArray<String>& denylist() const { return m_denylist; }
    bool isAllowed(const String& name) const;

private:
    bool isDenied(const String& name) const;

    EditorSession* m_session = nullptr;
    bool m_inSession = false;
    UInt64 m_sessionCheckpoint = 0;
    String m_sessionId;
    String m_sessionGoal;
    DynamicArray<String> m_allowlist;
    DynamicArray<String> m_denylist;
};

} // namespace cakery
