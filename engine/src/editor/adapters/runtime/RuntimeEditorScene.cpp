// do@Redlive

#include "RuntimeEditorBackend.h"

#include "runtime/core/asserts.h"
#include "runtime/core/context/system_context.h"
#include "runtime/core/debug/instrumentor.h"
#include "runtime/core/log/log_system.h"
#include "runtime/core/meta/component_db.h"
#include "runtime/function/script/script_runtime.h"
#include "runtime/function/script/script_system.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/world/components/id_component.h"
#include "runtime/function/world/components/tag_component.h"
#include "runtime/function/world/components/tilemap/tilemap_component.h"
#include "runtime/function/world/scene.h"
#include "runtime/function/world/world.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace dodoe;

namespace cakery {

namespace {

void SyncNativeComponents(dodoe::Entity entity, const std::vector<EditorComponent>& components) {
    auto& component_db = dodoe::ComponentDB::self();
    for (const auto& component : components) {
        if (component.typeName == "HierarchyComponent") continue;
        const auto* entry = component_db.find(
            dodoe::String(component.typeName.data(), component.typeName.size()));
        if (!entry || !entry->readJson) continue;
        if (!entry->contains(entity)) {
            if (!entry->canAdd()) continue;
            entry->add(entity);
        }
        void* ptr = entry->get(entity);
        if (!ptr) continue;
        (void)entry->readJson(ptr, component.value);
        if (entry->markDirty) {
            entry->markDirty(entity);
        }
    }
}

void SyncManagedComponents(dodoe::SystemContext& context, dodoe::Entity entity,
                           const std::vector<EditorComponent>& components)
{
    auto* scriptSystem = context.getScriptSystem();
    auto* scriptRuntime = scriptSystem ? scriptSystem->getScriptRuntime() : nullptr;
    if (!scriptRuntime) {
        return;
    }

    const std::uint64_t uuid = static_cast<std::uint64_t>(entity.uuid());
    dodoe::DynamicArray<dodoe::Pair<dodoe::String, dodoe::Json>> existing;
    scriptRuntime->getEntityManagedComponentFields(uuid, existing);

    std::unordered_set<std::string> desired;
    for (const auto& component : components) {
        desired.insert(component.typeName);
        bool present = false;
        for (const auto& [typeName, fields] : existing) {
            if (std::string(typeName.c_str()) == component.typeName) {
                present = true;
                break;
            }
        }
        if (!present) {
            scriptRuntime->addEntityManagedComponentFromManaged(
                uuid, dodoe::String(component.typeName.data(), component.typeName.size()));
        }
        scriptRuntime->setEntityManagedComponentFields(
            uuid,
            dodoe::String(component.typeName.data(), component.typeName.size()),
            component.value);
    }

    for (const auto& [typeName, fields] : existing) {
        if (!desired.contains(std::string(typeName.c_str()))) {
            scriptRuntime->removeEntityManagedComponentFromManaged(uuid, typeName);
        }
    }
}

} // anonymous namespace

bool RuntimeEditorBackend::reconcileScene(const EditorDocument& document)
{
    if (m_playState != "edit") {
        return true;
    }
    DO_PROFILE_SCOPE_CATEGORY("Cakery::reconcileScene", "boot");
    SystemContext* ctx = m_app ? &m_app->context() : nullptr;
    World* world = ctx ? ctx->getWorld() : nullptr;
    DO_ASSERT(world, "Cakery reconcile: world unavailable for document '{}'", document.name);
    if (!world) {
        return false;
    }
    Scene* scene = world->getActiveScene();
    if (!scene) {
        scene = world->createScene(document.name.empty()
            ? dodoe::String("Untitled")
            : dodoe::String(document.name.data(), document.name.size()));
        DO_ASSERT(scene, "Cakery reconcile: failed to create scene for '{}'", document.name);
        if (!scene) {
            return false;
        }
        world->setActiveScene(scene);
    } else if (!document.name.empty() && scene->getName().c_str() != document.name) {
        scene->setName(dodoe::String(document.name.data(), document.name.size()));
    }
    DO_INFO("Cakery reconcile: document '{}' ({} entities)", document.name, document.entities.size());

    for (const EditorEntity& entity : document.entities) {
        const dodoe::UUID uuid(entity.uuid);
        Entity sceneEntity = scene->tryGetEntityByUUID(uuid);
        if (!sceneEntity.valid()) {
            sceneEntity = scene->createEntity(
                uuid, dodoe::String(entity.name.data(), entity.name.size()));
        } else if (sceneEntity.name().c_str() != entity.name) {
            sceneEntity.getComponent<IDComponent>().setName(
                dodoe::String(entity.name.data(), entity.name.size()));
        }
        SyncNativeComponents(sceneEntity, entity.nativeComponents);
        SyncManagedComponents(*ctx, sceneEntity, entity.managedComponents);
    }
    DO_INFO("Cakery reconcile: {} entities synced", document.entities.size());

    for (Entity sceneEntity : scene->getEntities()) {
        if (sceneEntity.hasComponent<TagComponent>() &&
            sceneEntity.getComponent<TagComponent>().tag == "PrimaryCamera") {
            continue;
        }
        bool keep = false;
        for (const EditorEntity& entity : document.entities) {
            if (entity.uuid == static_cast<std::uint64_t>(sceneEntity.uuid())) {
                keep = true;
                break;
            }
        }
        if (!keep) {
            scene->destroyEntity(sceneEntity);
        }
    }

    rebuildHierarchy(*scene, document);
    return true;
}

void RuntimeEditorBackend::rebuildHierarchy(dodoe::Scene& scene, const EditorDocument& document)
{
    std::unordered_map<std::uint64_t, std::vector<const EditorEntity*>> childrenByParent;
    for (const auto& entity : document.entities) {
        std::uint64_t parent = entity.parent;
        if (parent == 0) {
            for (const auto& component : entity.nativeComponents) {
                if (component.typeName != "HierarchyComponent" ||
                    !component.value.contains("parent_uuid")) {
                    continue;
                }
                parent = component.value["parent_uuid"].get<std::uint64_t>();
                break;
            }
        }
        if (parent != 0) {
            childrenByParent[parent].push_back(&entity);
        }
    }

    for (const auto& [parentUuid, children] : childrenByParent) {
        dodoe::Entity parentEntity = scene.tryGetEntityByUUID(dodoe::UUID(parentUuid));
        if (!parentEntity.valid()) continue;
        if (!parentEntity.hasComponent<HierarchyComponent>()) {
            parentEntity.addComponent<HierarchyComponent>();
        }
        auto& parentHC = parentEntity.getComponent<HierarchyComponent>();
        parentHC.children.clear();

        std::vector<const EditorEntity*> ordered = children;
        if (parentEntity.hasComponent<TilemapComponent>()) {
            const EditorEntity* tilemapEntity = nullptr;
            for (const auto& entity : document.entities) {
                if (entity.uuid == parentUuid) {
                    tilemapEntity = &entity;
                    break;
                }
            }
            if (tilemapEntity) {
                std::vector<std::uint64_t> order;
                for (const auto& component : tilemapEntity->nativeComponents) {
                    if (component.typeName != "TilemapComponent") continue;
                    if (component.value.contains("layer_order") &&
                        component.value["layer_order"].is_array()) {
                        for (const auto& item : component.value["layer_order"]) {
                            if (item.is_number_unsigned()) {
                                order.push_back(item.get<std::uint64_t>());
                            }
                        }
                    }
                    break;
                }
                if (!order.empty()) {
                    ordered.clear();
                    for (const std::uint64_t uuid : order) {
                        for (const EditorEntity* child : children) {
                            if (child->uuid == uuid) {
                                ordered.push_back(child);
                                break;
                            }
                        }
                    }
                    for (const EditorEntity* child : children) {
                        if (std::find(ordered.begin(), ordered.end(), child) == ordered.end()) {
                            ordered.push_back(child);
                        }
                    }
                }
            }
        }

        for (const EditorEntity* childEntity : ordered) {
            dodoe::Entity child = scene.tryGetEntityByUUID(dodoe::UUID(childEntity->uuid));
            if (!child.valid()) continue;
            if (!child.hasComponent<HierarchyComponent>()) {
                child.addComponent<HierarchyComponent>();
            }
            auto& childHC = child.getComponent<HierarchyComponent>();
            childHC.parent = parentEntity;
            childHC.parent_uuid = parentEntity.uuid();
            childHC.dirty = true;
            parentHC.children.push_back(child);
        }
        parentHC.child_count = static_cast<int>(parentHC.children.size());
        parentHC.dirty = true;
    }
    DO_INFO("Cakery rebuildHierarchy: rebuilt {} parent link(s)", childrenByParent.size());
}

} // namespace cakery
