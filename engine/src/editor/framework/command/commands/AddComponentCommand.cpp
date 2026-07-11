// do@Redlive

#include "AddComponentCommand.h"
#include "framework/EditorContext.h"
#include "framework/core/UuidResolve.h"

#include "runtime/core/meta/component_db.h"
#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/log/log_system.h"

namespace cakery {

AddComponentCommand::AddComponentCommand(dodoe::Uuid entity, std::string componentName)
    : m_entity(entity)
    , m_componentName(std::move(componentName))
{}

bool AddComponentCommand::execute(EditorContext& ctx)
{
    auto* scene = ctx.activeScene();
    if (!scene) return false;

    auto entity = ResolveEntity(scene, m_entity);
    if (!entity.valid()) return false;

    auto& db = dodoe::ComponentDB::self();
    db.addComponent(entity, m_componentName);

    LOG_INFO("[AddComponent] {} to entity {}", m_componentName, static_cast<uint64_t>(m_entity));
    return true;
}

void AddComponentCommand::undo(EditorContext& ctx)
{
    auto* scene = ctx.activeScene();
    if (!scene) return;

    auto entity = ResolveEntity(scene, m_entity);
    if (!entity.valid()) return;

    auto& db = dodoe::ComponentDB::self();
    db.removeComponent(entity, m_componentName);
}

std::string AddComponentCommand::label() const
{
    return "Add " + m_componentName;
}

} // namespace cakery
