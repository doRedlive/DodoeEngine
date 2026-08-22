// do@Redlive

#include "ReparentEntityCommand.h"

#include "adapters/runtime/services/UuidResolve.h"
#include "core/console/CommandRegistry.h"
#include "core/document/EditorDocumentModel.h"
#include "core/EditorSession.h"

#include "runtime/core/context/system_context.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/scene.h"

#include <algorithm>
#include <memory>
#include <utility>

namespace cakery {

namespace {

dodoe::Scene* ActiveScene() {
    dodoe::World* world = dodoe::GetWorld();
    return world ? world->getActiveScene() : nullptr;
}

} // namespace

ReparentEntityCommand::ReparentEntityCommand(dodoe::UUID entity, dodoe::UUID oldParent, dodoe::UUID newParent)
    : m_entity(entity)
    , m_oldParent(oldParent)
    , m_newParent(newParent)
{}

void ReparentEntityCommand::execute(EditorDocumentModel& model)
{
    doReparent(model, m_newParent);
}

void ReparentEntityCommand::revert(EditorDocumentModel& model)
{
    doReparent(model, m_oldParent);
}

void ReparentEntityCommand::doReparent(EditorDocumentModel& model, dodoe::UUID newParent)
{
    auto* scene = ActiveScene();
    if (scene) {
        auto entity = ResolveEntity(scene, m_entity);
        if (entity.valid()) {
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

            if (newParent.isValid()) {
                auto parentEntity = ResolveEntity(scene, newParent);
                if (parentEntity.valid()) {
                    if (!parentEntity.hasComponent<dodoe::HierarchyComponent>()) {
                        parentEntity.addComponent<dodoe::HierarchyComponent>();
                    }
                    hc.parent = parentEntity;
                    hc.parent_uuid = parentEntity.uuid();
                    hc.dirty = true;

                    auto& parentHC = parentEntity.getComponent<dodoe::HierarchyComponent>();
                    parentHC.children.push_back(entity);
                    parentHC.child_count = static_cast<int>(parentHC.children.size());
                    parentHC.dirty = true;
                }
            }
            hc.dirty = true;
        }
    }

    model.reparentEntity(static_cast<std::uint64_t>(m_entity),
                         static_cast<std::uint64_t>(newParent));
}

std::string ReparentEntityCommand::label() const
{
    return "Reparent Entity";
}

void RegisterReparentCommand()
{
    static bool registered = false;
    if (registered) return;
    registered = true;

    auto& reg = CommandRegistry::self();

    reg.add({"entity.reparent", "Change entity parent",
             "entity.reparent <uuid> <parent_uuid>",
             {{"uuid", "uuid", "Entity UUID", true},
              {"parent", "uuid", "New parent UUID", true}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 if (args.positional.size() < 2) {
                     return CommandResult::Err("Usage: entity.reparent <uuid> <parent_uuid>");
                 }
                 dodoe::UUID childUuid = dodoe::UUID::FromString(args.positional[0]);
                 dodoe::UUID newParentUuid = dodoe::UUID::FromString(args.positional[1]);
                 auto* scene = ActiveScene();
                 if (!scene) return CommandResult::Err("No active scene");
                 auto child = ResolveEntity(scene, childUuid);
                 if (!child.valid()) return CommandResult::Err("Child entity not found");
                 dodoe::UUID oldParent;
                 if (child.hasComponent<dodoe::HierarchyComponent>()) {
                     oldParent = child.getComponent<dodoe::HierarchyComponent>().parent_uuid;
                 }
                 auto cmd = std::make_unique<ReparentEntityCommand>(childUuid, oldParent, newParentUuid);
                 auto* executed = session.history().execute(std::move(cmd), session.documentModel());
                 if (!executed) return CommandResult::Err("Failed to reparent");
                 session.notifyDocumentChanged();
                 return CommandResult::Ok("Reparented");
             }});
}

} // namespace cakery
