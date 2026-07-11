// do@Redlive

#include "RemoveComponentCommand.h"
#include "framework/EditorContext.h"
#include "framework/core/UuidResolve.h"

#include "runtime/core/meta/component_db.h"
#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/log/log_system.h"

namespace cakery {

RemoveComponentCommand::RemoveComponentCommand(dodoe::Uuid entity, std::string componentName)
    : m_entity(entity)
    , m_componentName(std::move(componentName))
{}

bool RemoveComponentCommand::execute(EditorContext& ctx)
{
    auto* scene = ctx.activeScene();
    if (!scene) return false;

    auto entity = ResolveEntity(scene, m_entity);
    if (!entity.valid()) return false;

    auto& db = dodoe::ComponentDB::self();
    void* compPtr = db.getComponentPtr(entity, m_componentName);
    if (compPtr) {
        auto* entry = db.find(m_componentName);
        if (entry && entry->writeJson) {
            m_serializedComponent = entry->writeJson(compPtr);
        }
    }

    db.removeComponent(entity, m_componentName);

    LOG_INFO("[RemoveComponent] {} from entity {}", m_componentName, static_cast<uint64_t>(m_entity));
    return true;
}

void RemoveComponentCommand::undo(EditorContext& ctx)
{
    auto* scene = ctx.activeScene();
    if (!scene) return;

    auto entity = ResolveEntity(scene, m_entity);
    if (!entity.valid()) return;

    auto& db = dodoe::ComponentDB::self();
    db.addComponent(entity, m_componentName);

    void* compPtr = db.getComponentPtr(entity, m_componentName);
    if (compPtr) {
        auto* entry = db.find(m_componentName);
        if (entry && entry->readJson) {
            entry->readJson(compPtr, m_serializedComponent);
        }
    }
}

std::string RemoveComponentCommand::label() const
{
    return "Remove " + m_componentName;
}

} // namespace cakery
