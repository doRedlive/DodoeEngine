// do@Redlive

#include "CreateEntityCommand.h"
#include "framework/EditorContext.h"
#include "framework/core/UuidResolve.h"

#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/log/log_system.h"

namespace cakery {

CreateEntityCommand::CreateEntityCommand(dodoe::UUID uuid, std::string name,
                                         std::optional<dodoe::UUID> parent)
    : m_uuid(uuid)
    , m_name(std::move(name))
    , m_parent(std::move(parent))
{}

bool CreateEntityCommand::execute(EditorContext& ctx)
{
    auto* scene = ctx.activeScene();
    if (!scene) return false;

    auto entity = scene->createEntity(m_uuid, dodoe::String(m_name.c_str()));
    if (!entity.valid()) return false;

    if (m_parent.has_value()) {
        auto parentEntity = ResolveEntity(scene, *m_parent);
        if (parentEntity.valid()) {
            if (!entity.hasComponent<dodoe::HierarchyComponent>()) {
                entity.addComponent<dodoe::HierarchyComponent>();
            }
            if (!parentEntity.hasComponent<dodoe::HierarchyComponent>()) {
                parentEntity.addComponent<dodoe::HierarchyComponent>();
            }

            auto& childHC = entity.getComponent<dodoe::HierarchyComponent>();
            childHC.parent = parentEntity;
            childHC.parent_uuid = parentEntity.uuid();
            childHC.dirty = true;

            auto& parentHC = parentEntity.getComponent<dodoe::HierarchyComponent>();
            parentHC.children.push_back(entity);
            parentHC.child_count = static_cast<int>(parentHC.children.size());
            parentHC.dirty = true;
        }
    }

    LOG_INFO("[CreateEntity] {} ({})", m_name, static_cast<uint64_t>(m_uuid));
    return true;
}

void CreateEntityCommand::undo(EditorContext& ctx)
{
    auto* scene = ctx.activeScene();
    if (!scene) return;

    auto entity = ResolveEntity(scene, m_uuid);
    if (entity.valid()) {
        scene->destroyEntity(entity);
    }
}

void CreateEntityCommand::redo(EditorContext& ctx)
{
    execute(ctx);
}

std::string CreateEntityCommand::label() const
{
    return "Create " + m_name;
}

} // namespace cakery
