// do@Redlive
#pragma once
#include "dopch.h"

namespace cakery {

class EditorSession;

class ToolActionController {
public:
    explicit ToolActionController(EditorSession& session)
        : m_session(session) {}

    bool listToolActions(DynamicArray<String>& out) const;
    bool invokeToolAction(const String& path);

private:
    EditorSession& m_session;
};

} // namespace cakery
