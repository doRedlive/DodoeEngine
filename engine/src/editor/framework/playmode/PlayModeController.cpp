// do@Redlive

#include "PlayModeController.h"
#include "framework/EditorContext.h"
#include "framework/command/CommandStack.h"

#include "runtime/function/world/world.h"
#include "runtime/function/world/scene.h"
#include "runtime/resource/res_type/scene_res.h"
#include "runtime/function/log/log_system.h"

namespace cakery {

void PlayModeController::play()
{
    auto* scene = m_ctx.activeScene();
    auto* world = m_ctx.world();
    if (!scene || !world) return;

    auto snap = std::make_unique<dodoe::SceneRes>(scene->serialize());
    m_snapshot = std::move(snap);

    world->setState(dodoe::WorldState::Runtime);
    m_state = PlayState::Playing;
    stateChanged.fire(m_state);

    LOG_INFO("[PlayMode] Entered Play mode");
}

void PlayModeController::pause()
{
    auto* world = m_ctx.world();
    if (!world || m_state != PlayState::Playing) return;

    world->setState(dodoe::WorldState::Pause);
    m_state = PlayState::Paused;
    stateChanged.fire(m_state);
}

void PlayModeController::resume()
{
    auto* world = m_ctx.world();
    if (!world || m_state != PlayState::Paused) return;

    world->setState(dodoe::WorldState::Runtime);
    m_state = PlayState::Playing;
    stateChanged.fire(m_state);
}

void PlayModeController::stop()
{
    auto* scene = m_ctx.activeScene();
    auto* world = m_ctx.world();
    if (!scene || !world) return;

    world->setState(dodoe::WorldState::Simulation);

    if (m_snapshot) {
        scene->deserialize(*m_snapshot);
        m_snapshot.reset();
    }

    m_ctx.commands().clear();

    m_state = PlayState::Edit;
    stateChanged.fire(m_state);

    LOG_INFO("[PlayMode] Returned to Edit mode");
}

} // namespace cakery
