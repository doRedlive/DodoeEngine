// do@Redlive

#include "dopch.h"

#include "ToolActionController.h"

#include "core/EditorSession.h"

namespace cakery {

bool ToolActionController::listToolActions(DynamicArray<String>& out) const
{
    return m_session.listToolActions(out);
}

bool ToolActionController::invokeToolAction(const String& path)
{
    return m_session.invokeToolAction(path);
}

} // namespace cakery
