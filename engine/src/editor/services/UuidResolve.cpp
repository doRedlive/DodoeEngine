// do@Redlive

#include "dopch.h"

#include "UuidResolve.h"

#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/core/log/log_system.h"

namespace cakery {

dodoe::Entity ResolveEntity(dodoe::Scene* scene, dodoe::UUID uuid)
{
    if (!scene || !uuid.isValid()) {
        return {};
    }

    auto entity = scene->tryGetEntityByUUID(uuid);
    if (!entity.valid()) {
        LOG_WARN("[Editor] ResolveEntity: entity not found for uuid {}", static_cast<UInt64>(uuid));
    }

    return entity;
}

} // namespace cakery
