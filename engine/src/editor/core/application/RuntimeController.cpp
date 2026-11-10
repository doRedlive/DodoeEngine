// do@Redlive

#include "dopch.h"

#include "RuntimeController.h"

#include "core/EditorSession.h"

namespace cakery {

bool RuntimeController::bootEngine()
{
    return m_session.bootEngine();
}

void RuntimeController::tick()
{
    m_session.tick();
}

void RuntimeController::shutdown()
{
    m_session.shutdown();
}

String RuntimeController::diagnostic() const
{
    return m_session.diagnostic();
}

bool RuntimeController::applyRenderSettingsOverlay(const Json& renderSettings)
{
    return m_session.applyRenderSettingsOverlay(renderSettings);
}

} // namespace cakery
