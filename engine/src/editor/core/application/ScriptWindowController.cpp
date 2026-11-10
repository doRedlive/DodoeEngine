// do@Redlive

#include "dopch.h"

#include "ScriptWindowController.h"

#include "core/EditorSession.h"

namespace cakery {

bool ScriptWindowController::listEditorWindows(DynamicArray<Pair<String, String>>& out) const
{
    return m_session.listEditorWindows(out);
}

bool ScriptWindowController::getEditorWindowUI(const String& id, Json& out) const
{
    return m_session.getEditorWindowUI(id, out);
}

bool ScriptWindowController::dispatchEditorEvent(const String& owner, const String& ownerId,
                                                 const String& controlId,
                                                 const String& eventName, const Json& value)
{
    return m_session.dispatchEditorEvent(owner, ownerId, controlId, eventName, value);
}

} // namespace cakery
