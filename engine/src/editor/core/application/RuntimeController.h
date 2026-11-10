// do@Redlive
#pragma once
#include "dopch.h"

namespace cakery {

class EditorSession;

class RuntimeController {
public:
    explicit RuntimeController(EditorSession& session)
        : m_session(session) {}

    bool bootEngine();
    void tick();
    void shutdown();
    String diagnostic() const;
    bool applyRenderSettingsOverlay(const Json& renderSettings);

private:
    EditorSession& m_session;
};

} // namespace cakery
