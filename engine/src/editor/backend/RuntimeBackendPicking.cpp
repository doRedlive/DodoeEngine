// do@Redlive
#include "dopch.h"
#include "RuntimeBackend.h"
#include "EditorCamera.h"
#include "runtime/core/context/system_context.h"
#include "runtime/service/editor/picking_backend.h"
#include "runtime/function/world/scene.h"
#include "runtime/function/world/world.h"
#include <cmath>

using namespace dodoe;
namespace cakery {
void RuntimeBackend::pickAt(float screenX, float screenY)
{
    if (!m_camera) {
        return;
    }
    SystemContext* ctx = m_app ? &m_app->context() : nullptr;
    World* world = ctx ? ctx->getWorld() : nullptr;
    if (!world) {
        return;
    }
    Scene* scene = world->getActiveScene();
    if (!scene) {
        return;
    }
    Vector3f origin, dir;
    m_camera->screenToRay(screenX, screenY, origin, dir);
    Entity entity = PickingBackend::RaycastNearest(*scene, origin, dir);
    setSelectedUuid(entity.valid() ? static_cast<UInt64>(entity.uuid()) : 0);
}

void RuntimeBackend::requestPick(float screenX, float screenY)
{
    if (!m_camera) {
        return;
    }
    auto& channel = GetPickChannel().get<PickChannelData>();
    m_pickToggle = m_ctrlHeld;
    m_pickAdd = m_shiftHeld;
    const float dpr = m_surface.devicePixelRatio > 0.0f ? m_surface.devicePixelRatio : 1.0f;
    channel.request.x = static_cast<Int32>(std::lround(screenX * dpr));
    channel.request.y = static_cast<Int32>(std::lround(screenY * dpr));
    channel.request.sequence = ++m_pick_sequence;
    DO_INFO("EditorPick: request ({}, {}) seq={}", channel.request.x, channel.request.y, channel.request.sequence);
}

void RuntimeBackend::setSelectedUuid(UInt64 uuid)
{
    if (uuid != m_selectedUuid) m_sceneGizmos.cancelDrag(sceneGizmoContext());
    const bool toggle = m_pickToggle, add = m_pickAdd;
    m_pickToggle = m_pickAdd = false;
    if (toggle || add) {
        if (uuid != 0) m_eventCallback(BackendEventMessage{
            toggle ? "selection_toggle" : "selection_add", ToString(uuid)});
        return;
    }
    if (uuid == 0) {
        if (m_selectedUuid != 0) {
            m_selectedUuid = 0;
            m_sceneGizmos.resetHover();
            m_eventCallback(BackendEventMessage{"selection_changed", String()});
        }
        return;
    }
    m_selectedUuid = uuid;
    m_sceneGizmos.resetHover();
    m_eventCallback(BackendEventMessage{"selection_changed", ToString(uuid)});
}

Entity RuntimeBackend::selectedSceneEntity() const
{
    SystemContext* ctx = m_app ? &m_app->context() : nullptr;
    World* world = ctx ? ctx->getWorld() : nullptr;
    if (!world) {
        return {};
    }
    Scene* scene = world->getActiveScene();
    if (!scene || m_selectedUuid == 0) {
        return {};
    }
    return scene->tryGetEntityByUUID(dodoe::UUID(m_selectedUuid));
}

} // namespace cakery
