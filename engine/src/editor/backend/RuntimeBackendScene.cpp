// do@Redlive

#include "dopch.h"

#include "RuntimeBackend.h"

#include "services/PrefabService.h"

#include "runtime/core/asserts.h"
#include "runtime/core/context/system_context.h"
#include "runtime/core/debug/instrumentor.h"
#include "runtime/core/log/log_system.h"
#include "runtime/core/meta/component_db.h"
#include "runtime/function/script/script_runtime.h"
#include "runtime/function/script/script_system.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/world/components/id_component.h"
#include "runtime/function/world/components/prefab_instance_component.h"
#include "runtime/function/world/components/prefab_node_component.h"
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

void SyncNativeComponents(Entity entity, const DynamicArray<EditorComponent>& components) {
    auto& component_db = ComponentDB::self();
    for (const auto& component : components) {
        if (component.typeName == "HierarchyComponent") continue;
        const auto* entry = component_db.find(
            String(component.typeName.data(), component.typeName.size()));
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

void SyncManagedComponents(SystemContext& context, Entity entity,
                           const DynamicArray<EditorComponent>& components)
{
    auto* scriptSystem = context.getScriptSystem();
    auto* scriptRuntime = scriptSystem ? scriptSystem->getScriptRuntime() : nullptr;
    if (!scriptRuntime) {
        return;
    }

    const UInt64 uuid = static_cast<UInt64>(entity.uuid());
    DynamicArray<Pair<String, Json>> existing;
    scriptRuntime->getEntityManagedComponentFields(uuid, existing);

    UnorderedSet<String> desired;
    for (const auto& component : components) {
        desired.insert(component.typeName);
        bool present = false;
        for (const auto& [typeName, fields] : existing) {
            if (String(typeName) == component.typeName) {
                present = true;
                break;
            }
        }
        if (!present) {
            scriptRuntime->addEntityManagedComponentFromManaged(
                uuid, String(component.typeName.data(), component.typeName.size()));
        }
        scriptRuntime->setEntityManagedComponentFields(
            uuid,
            String(component.typeName.data(), component.typeName.size()),
            component.value);
    }

    for (const auto& [typeName, fields] : existing) {
        if (!desired.contains(String(typeName))) {
            scriptRuntime->removeEntityManagedComponentFromManaged(uuid, typeName);
        }
    }
}

} // anonymous namespace

bool RuntimeBackend::reconcileScene(const EditorDocument& document)
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
            ? String("Untitled")
            : String(document.name.data(), document.name.size()));
        DO_ASSERT(scene, "Cakery reconcile: failed to create scene for '{}'", document.name);
        if (!scene) {
            return false;
        }
        world->setActiveScene(scene);
    } else if (!document.name.empty() && scene->getName() != document.name) {
        scene->setName(String(document.name.data(), document.name.size()));
    }
    DO_INFO("Cakery reconcile: document '{}' ({} entities)", document.name, document.entities.size());

    for (const EditorEntity& entity : document.entities) {
        const UUID uuid(entity.uuid);
        Entity sceneEntity = scene->tryGetEntityByUUID(uuid);
        if (!sceneEntity.valid()) {
            sceneEntity = scene->createEntity(
                uuid, String(entity.name.data(), entity.name.size()));
        } else if (sceneEntity.name() != entity.name) {
            sceneEntity.getComponent<IDComponent>().setName(
                String(entity.name.data(), entity.name.size()));
        }
        SyncNativeComponents(sceneEntity, entity.nativeComponents);
        SyncManagedComponents(*ctx, sceneEntity, entity.managedComponents);
    }
    DO_INFO("Cakery reconcile: {} entities synced", document.entities.size());

    if (m_tilePaint && m_tilePaint->hasTarget()) {
        const UUID tilemap_uuid = m_tilePaint->activeTilemap();
        if (!scene->tryGetEntityByUUID(tilemap_uuid).valid()) {
            m_tilePaint->setActiveTilemap(UUID(0));
            m_tilePaint->setActiveLayer(UUID(0));
            m_tilePaintActive = false;
        }
    }

    for (Entity sceneEntity : scene->getEntities()) {
        if (sceneEntity.valid() && sceneEntity.hasComponent<PrefabInstanceComponent>() &&
            !sceneEntity.hasComponent<PrefabNodeComponent>()) {
            ExpandPrefabInstance(sceneEntity);
        }
    }

    for (Entity sceneEntity : scene->getEntities()) {
        if (sceneEntity.hasComponent<TagComponent>() &&
            sceneEntity.getComponent<TagComponent>().tag == "PrimaryCamera") {
            continue;
        }
        if (sceneEntity.hasComponent<PrefabNodeComponent>()) {
            continue;
        }
        bool keep = false;
        for (const EditorEntity& entity : document.entities) {
            if (entity.uuid == static_cast<UInt64>(sceneEntity.uuid())) {
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

void RuntimeBackend::rebuildHierarchy(Scene& scene, const EditorDocument& document)
{
    UnorderedMap<UInt64, DynamicArray<const EditorEntity*>> childrenByParent;
    for (const auto& entity : document.entities) {
        UInt64 parent = entity.parent;
        if (parent == 0) {
            for (const auto& component : entity.nativeComponents) {
                if (component.typeName != "HierarchyComponent" ||
                    !component.value.contains("parent_uuid")) {
                    continue;
                }
                parent = component.value["parent_uuid"].get<UInt64>();
                break;
            }
        }
        if (parent != 0) {
            childrenByParent[parent].push_back(&entity);
        }
    }

    for (const auto& [parentUuid, children] : childrenByParent) {
        Entity parentEntity = scene.tryGetEntityByUUID(UUID(parentUuid));
        if (!parentEntity.valid()) continue;
        if (!parentEntity.hasComponent<HierarchyComponent>()) {
            parentEntity.addComponent<HierarchyComponent>();
        }
        auto& parentHC = parentEntity.getComponent<HierarchyComponent>();
        DynamicArray<Entity> prefabChildren;
        for (Entity child : parentHC.children) {
            if (child.valid() && child.hasComponent<PrefabNodeComponent>()) {
                prefabChildren.push_back(child);
            }
        }
        parentHC.children.clear();

        DynamicArray<const EditorEntity*> ordered = children;
        if (parentEntity.hasComponent<TilemapComponent>()) {
            const EditorEntity* tilemapEntity = nullptr;
            for (const auto& entity : document.entities) {
                if (entity.uuid == parentUuid) {
                    tilemapEntity = &entity;
                    break;
                }
            }
            if (tilemapEntity) {
                DynamicArray<UInt64> order;
                for (const auto& component : tilemapEntity->nativeComponents) {
                    if (component.typeName != "TilemapComponent") continue;
                    if (component.value.contains("layer_order") &&
                        component.value["layer_order"].is_array()) {
                        for (const auto& item : component.value["layer_order"]) {
                            if (item.is_number_unsigned()) {
                                order.push_back(item.get<UInt64>());
                            }
                        }
                    }
                    break;
                }
                if (!order.empty()) {
                    ordered.clear();
                    for (const UInt64 uuid : order) {
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
            Entity child = scene.tryGetEntityByUUID(UUID(childEntity->uuid));
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
        for (Entity child : prefabChildren) {
            parentHC.children.push_back(child);
        }
        parentHC.child_count = static_cast<int>(parentHC.children.size());
        parentHC.dirty = true;
    }
    DO_INFO("Cakery rebuildHierarchy: rebuilt {} parent link(s)", childrenByParent.size());
}

} // namespace cakery
