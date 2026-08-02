// do@Redlive

#include "ReparentEntityCommand.h"
#include "framework/EditorContext.h"
#include "framework/core/UuidResolve.h"

#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components/hierarchy_component.h"

namespace cakery {

ReparentEntityCommand::ReparentEntityCommand(dodoe::UUID entity, dodoe::UUID oldParent, dodoe::UUID newParent)
    : m_entity(entity)
    , m_oldParent(oldParent)
    , m_newParent(newParent)
{}

bool ReparentEntityCommand::execute(EditorContext& ctx)
{
    auto* scene = ctx.activeScene();
    if (!scene) return false;

    auto entity = ResolveEntity(scene, m_entity);
    if (!entity.valid()) return false;

    if (!entity.hasComponent<dodoe::HierarchyComponent>()) {
        entity.addComponent<dodoe::HierarchyComponent>();
    }

    auto& hc = entity.getComponent<dodoe::HierarchyComponent>();

    if (hc.parent.valid() && hc.parent.hasComponent<dodoe::HierarchyComponent>()) {
        auto& oldParentHC = hc.parent.getComponent<dodoe::HierarchyComponent>();
        auto& siblings = oldParentHC.children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), entity), siblings.end());
        oldParentHC.child_count = static_cast<int>(siblings.size());
        oldParentHC.dirty = true;
    }

    hc.parent = {};
    hc.parent_uuid = dodoe::UUID(0);

    if (m_newParent.isValid()) {
        auto newParent = ResolveEntity(scene, m_newParent);
        if (newParent.valid()) {
            if (!newParent.hasComponent<dodoe::HierarchyComponent>()) {
                newParent.addComponent<dodoe::HierarchyComponent>();
            }
            hc.parent = newParent;
            hc.parent_uuid = newParent.uuid();
            hc.dirty = true;

            auto& newParentHC = newParent.getComponent<dodoe::HierarchyComponent>();
            newParentHC.children.push_back(entity);
            newParentHC.child_count = static_cast<int>(newParentHC.children.size());
            newParentHC.dirty = true;
        }
    }
    hc.dirty = true;

    return true;
}

void ReparentEntityCommand::undo(EditorContext& ctx)
{
    std::swap(m_oldParent, m_newParent);
    execute(ctx);
    std::swap(m_oldParent, m_newParent);
}

std::string ReparentEntityCommand::label() const
{
    return "Reparent Entity";
}

} // namespace cakery
