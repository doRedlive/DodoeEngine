// do@Redlive

#include "RenameEntityCommand.h"
#include "framework/EditorContext.h"
#include "framework/core/UuidResolve.h"

#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components/id_component.h"

namespace cakery {

RenameEntityCommand::RenameEntityCommand(dodoe::Uuid entity, std::string oldName, std::string newName)
    : m_entity(entity)
    , m_oldName(std::move(oldName))
    , m_newName(std::move(newName))
{}

bool RenameEntityCommand::execute(EditorContext& ctx)
{
    auto* scene = ctx.activeScene();
    if (!scene) return false;

    auto entity = ResolveEntity(scene, m_entity);
    if (!entity.valid()) return false;

    if (entity.hasComponent<dodoe::IDComponent>()) {
        entity.getComponent<dodoe::IDComponent>().name = m_newName;
    }

    return true;
}

void RenameEntityCommand::undo(EditorContext& ctx)
{
    auto* scene = ctx.activeScene();
    if (!scene) return;

    auto entity = ResolveEntity(scene, m_entity);
    if (!entity.valid()) return;

    if (entity.hasComponent<dodoe::IDComponent>()) {
        entity.getComponent<dodoe::IDComponent>().name = m_oldName;
    }
}

std::string RenameEntityCommand::label() const
{
    return "Rename to " + m_newName;
}

} // namespace cakery
