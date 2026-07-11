// do@Redlive

#include "DeleteEntityCommand.h"
#include "framework/EditorContext.h"
#include "framework/core/UuidResolve.h"

#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/resource/res_type/scene_res.h"
#include "runtime/function/log/log_system.h"

namespace cakery {

DeleteEntityCommand::DeleteEntityCommand(dodoe::Uuid uuid)
    : m_uuid(uuid)
{}

bool DeleteEntityCommand::execute(EditorContext& ctx)
{
    auto* scene = ctx.activeScene();
    if (!scene) return false;

    auto entity = ResolveEntity(scene, m_uuid);
    if (!entity.valid()) return false;

    m_serializedSubtree = std::make_unique<dodoe::SceneRes>(scene->serialize());

    scene->destroyEntity(entity);

    LOG_INFO("[DeleteEntity] {}", static_cast<uint64_t>(m_uuid));
    return true;
}

void DeleteEntityCommand::undo(EditorContext& ctx)
{
    if (!m_serializedSubtree) return;

    auto* scene = ctx.activeScene();
    if (!scene) return;

    scene->deserialize(*m_serializedSubtree);
}

std::string DeleteEntityCommand::label() const
{
    return "Delete Entity";
}

} // namespace cakery
