// do@Redlive

#include "ReparentEntityCommand.h"
#include "framework/EditorContext.h"
#include "framework/core/UuidResolve.h"

#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components/hierarchy_component.h"

namespace cakery {

ReparentEntityCommand::ReparentEntityCommand(dodoe::Uuid entity, dodoe::Uuid oldParent, dodoe::Uuid newParent)
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
        auto& siblings = hc.parent.getComponent<dodoe::HierarchyComponent>().children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), entity), siblings.end());
    }

    if (m_newParent.isValid()) {
        auto newParent = ResolveEntity(scene, m_newParent);
        if (newParent.valid()) {
            if (!newParent.hasComponent<dodoe::HierarchyComponent>()) {
                newParent.addComponent<dodoe::HierarchyComponent>();
            }
            hc.parent = newParent;
            newParent.getComponent<dodoe::HierarchyComponent>().children.push_back(entity);
        }
    } else {
        hc.parent = {};
    }

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
