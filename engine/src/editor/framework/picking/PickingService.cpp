// do@Redlive

#include "PickingService.h"
#include "framework/EditorContext.h"
#include "framework/camera/EditorCamera.h"

#include "runtime/function/world/scene.h"
#include "runtime/service/editor/picking_backend.h"

namespace cakery {

std::optional<dodoe::UUID> PickingService::pick(float screenX, float screenY)
{
    auto* scene = m_ctx.activeScene();
    if (!scene) return std::nullopt;

    dodoe::Vector3f origin, dir;
    m_ctx.camera().screenToRay(screenX, screenY, origin, dir);

    auto entity = dodoe::PickingBackend::RaycastNearest(*scene, origin, dir);
    if (!entity.valid()) return std::nullopt;

    return entity.uuid();
}

std::vector<dodoe::UUID> PickingService::pickRect(float x0, float y0, float x1, float y1)
{
    (void)x0; (void)y0; (void)x1; (void)y1;
    return {};
}

} // namespace cakery
