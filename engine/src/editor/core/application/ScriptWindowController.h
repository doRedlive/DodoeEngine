// do@Redlive
#pragma once
#include "dopch.h"

namespace cakery {

class EditorSession;

class ScriptWindowController {
public:
    explicit ScriptWindowController(EditorSession& session)
        : m_session(session) {}

    bool listEditorWindows(DynamicArray<Pair<String, String>>& out) const;
    bool getEditorWindowUI(const String& id, Json& out) const;
    bool dispatchEditorEvent(const String& owner, const String& ownerId,
                             const String& controlId, const String& eventName,
                             const Json& value);

private:
    EditorSession& m_session;
};

} // namespace cakery
