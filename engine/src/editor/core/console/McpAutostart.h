// do@Redlive
#pragma once
#include "dopch.h"

namespace cakery {

class EditorSession;

class McpAutostart {
public:
    McpAutostart(int argc, char** argv);

    void startIfConfigured(EditorSession& session) const;

private:
    bool m_requested = false;
    UInt16 m_port = 0;
};

} // namespace cakery
