// do@Redlive

#include "scene.h"

#include "world.h"
#include "entity.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/core/meta/component_db.h"
#include "runtime/core/project/project.h"
#include "runtime/core/utils/common.h"
#include "runtime/function/script/script_runtime.h"
#include "runtime/function/world/components.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/resource/res_type/scene_res.h"
#include "runtime/service/world/scene_importer.h"

namespace dodoe {

    namespace {

        ScriptRuntime* GetScriptRuntime() {
            if (!GetScriptSystem()) {
                return nullptr;
            }
            return GetScriptSystem()->getScriptRuntime();
        }

        bool ParseJsonText(const String& json_text, Json& out_json) {
            try {
                out_json = Json::parse(json_text);
                return true;
            }
            catch (const Json::exception&) {
                return false;
            }
        }

        std::vector<ComponentRes> SerializeNativeComponentsFiltered(Entity entity, const UnorderedSet<String>& only) {
            std::vector<ComponentRes> components;
            auto& component_db = ComponentDB::self();
            for (const auto& entry : component_db.entries()) {
                if (!only.empty() && only.find(entry.name) == only.end()) {
                    continue;
                }
                if (!entry.contains(entity) || !entry.writeJson) {
                    continue;
                }

                void* component_ptr = entry.get(entity);
                if (!component_ptr) {
                    continue;
                }

                ComponentRes component_res;
                component_res.m_type_name = entry.name;
                component_res.m_component = entry.writeJson(component_ptr).dump();
                components.push_back(std::move(component_res));
            }
            return components;
        }

        std::vector<ComponentRes> SerializeNativeComponents(Entity entity) {
            return SerializeNativeComponentsFiltered(entity, {});
        }

        std::vector<ComponentRes> SerializeManagedComponents(Entity entity) {
            std::vector<ComponentRes> components;
            ScriptRuntime* runtime = GetScriptRuntime();
            if (!runtime) {
                return components;
            }

            DynamicArray<Pair<String, Json>> managed_components;
            if (!runtime->getEntityManagedComponentFields(static_cast<uint64_t>(entity.uuid()), managed_components)) {
                return components;
            }

            for (const auto& [type_name, fields] : managed_components) {
                ComponentRes component_res;
                component_res.m_type_name = type_name;
                component_res.m_component = fields.dump();
                components.push_back(std::move(component_res));
            }

            return components;
        }

        void DeserializeNativeComponents(const std::vector<ComponentRes>& components, Entity entity) {
            auto& component_db = ComponentDB::self();
            for (const auto& component_res : components) {
                const auto* entry = component_db.find(component_res.m_type_name);
                if (!entry || !entry->readJson) {
                    continue;
                }

                if (!entry->contains(entity) && entry->add) {
                    entry->add(entity);
                }

                void* component_ptr = entry->get(entity);
                if (!component_ptr) {
                    continue;
                }

                Json component_json;
                if (!ParseJsonText(component_res.m_component, component_json)) {
                    continue;
                }

                (void)entry->readJson(component_ptr, component_json);
            }
        }

        void DeserializeManagedComponents(const std::vector<ComponentRes>& components, Entity entity) {
            ScriptRuntime* runtime = GetScriptRuntime();
            if (!runtime) {
                return;
            }

            const uint64_t uuid = static_cast<uint64_t>(entity.uuid());
            for (const auto& component_res : components) {
                runtime->addEntityManagedComponentFromManaged(uuid, component_res.m_type_name);

                if (component_res.m_component.empty()) {
                    continue;
                }
                Json fields;
                if (!ParseJsonText(component_res.m_component, fields)) {
                    continue;
                }
                runtime->setEntityManagedComponentFields(uuid, component_res.m_type_name, fields);
            }
        }

        void CollectSubtree(Entity root, std::vector<Entity>& out) {
            out.push_back(root);
            if (!root.hasComponent<HierarchyComponent>()) {
                return;
            }
            for (Entity child : root.getComponent<HierarchyComponent>().children) {
                if (child.valid()) {
                    CollectSubtree(child, out);
                }
            }
        }

    } // namespace

    Scene::Scene(const SceneCreateInfo& info) : Scene(info.world, info.name) { }

    Scene::Scene(World& world, const String& name) : m_world(world), m_name(name), m_reg(this) { }

    void Scene::save() {
        DO_PROFILE_SCOPE_CATEGORY("Scene::save", "startup");
        const auto active_project = Project::ActiveProject();
        if (!active_project) {
            DO_ERROR("Cannot save scene '{}' without active project.", m_name);
            return;
        }

        auto* asset_manager = ResourceManager::Self().getAssetManager();
        if (!asset_manager) {
            DO_ERROR("Cannot save scene '{}' because AssetManager is not initialized.", m_name);
            return;
        }

        const String asset_url((FsPath("Scenes") / (m_name + ".doscn")).generic_string().c_str());
        (void)asset_manager->saveAssetFile(serialize(), asset_url);
    }

    void Scene::onCreate() {

    }

    void Scene::onDelete() {

    }

    bool Scene::initialize(const SceneCreateInfo& info) {
        (void)info;
        auto entity = createEntity("Primary Camera"); 
        auto& camera = entity.addComponent<CameraComponent>();
        entity.getComponent<TagComponent>().setTag("PrimaryCamera");

        return true;
    }

    void Scene::shutdown() {
        m_reg.clear();
    }

    void Scene::onRuntimeStart() {
        m_world.onRuntimeStart(m_reg);
    }

    void Scene::onRuntimeUpdate(const float delta_time) {
        DO_PROFILE_SCOPE_CATEGORY("Scene::onRuntimeUpdate", "frame");
        m_world.onRuntimeUpdate(m_reg, delta_time);
    }

    void Scene::onRuntimeStop() {
        m_world.onRuntimeFinalize(m_reg);
    }

    void Scene::onSimulationStart() {
        m_world.onSimulationStart(m_reg);
    }

    void Scene::onSimulationStop() {
        m_world.onSimulationFinalize(m_reg);
    }

    void Scene::onSimulationUpdate(const float dt) {
        DO_PROFILE_SCOPE_CATEGORY("Scene::onSimulationUpdate", "frame");
        m_world.onSimulationUpdate(m_reg, dt);
    }

    SceneRes Scene::serialize() const {
        DO_PROFILE_SCOPE_CATEGORY("Scene::serialize", "startup");
        SceneRes scene_res;
        scene_res.m_name = getName();

        UnorderedSet<UUID> prefab_roots;
        UnorderedSet<UUID> prefab_internal;
        std::function<void(Entity)> collect_internal = [&](Entity entity) {
            if (!entity.valid() || !entity.hasComponent<HierarchyComponent>()) return;
            for (Entity child : entity.getComponent<HierarchyComponent>().children) {
                if (!child.valid()) continue;
                if (prefab_internal.insert(child.uuid()).second) {
                    collect_internal(child);
                }
            }
        };
        for (Entity entity : const_cast<Scene*>(this)->getEntities()) {
            if (entity.hasComponent<PrefabInstanceComponent>()) {
                prefab_roots.insert(entity.uuid());
                collect_internal(entity);
            }
        }

        for (Entity entity : const_cast<Scene*>(this)->getEntities()) {
            if (prefab_internal.find(entity.uuid()) != prefab_internal.end()) {
                continue;
            }

            EntityRes entity_res;
            entity_res.m_uuid = entity.uuid();
            entity_res.m_name = entity.name();
            if (prefab_roots.find(entity.uuid()) != prefab_roots.end()) {
                auto& inst = entity.getComponent<PrefabInstanceComponent>();
                if (entity.hasComponent<TransformComponent>()) {
                    const auto& tc = entity.getComponent<TransformComponent>();
                    inst.position = tc.position;
                    inst.rotation = tc.rotation;
                    inst.scale = tc.scale;
                }
                entity_res.m_native_components = SerializeNativeComponentsFiltered(
                    entity, {String("IDComponent"), String("PrefabInstanceComponent")});
            } else {
                entity_res.m_native_components = SerializeNativeComponents(entity);
            }
            entity_res.m_managed_components = SerializeManagedComponents(entity);
            scene_res.m_entities.push_back(std::move(entity_res));
        }

        return scene_res;
    }

    SceneRes Scene::serializeSubtree(Entity root) const {
        SceneRes scene_res;
        if (!root.valid()) {
            return scene_res;
        }

        std::vector<Entity> subtree;
        CollectSubtree(root, subtree);

        std::unordered_set<UUID> in_subtree;
        in_subtree.reserve(subtree.size());
        for (Entity entity : subtree) {
            in_subtree.insert(entity.uuid());
        }

        scene_res.m_name = root.name();
        for (Entity entity : subtree) {
            EntityRes entity_res;
            entity_res.m_uuid = entity.uuid();
            entity_res.m_name = entity.name();
            entity_res.m_native_components = SerializeNativeComponents(entity);
            entity_res.m_managed_components = SerializeManagedComponents(entity);

            if (entity.hasComponent<HierarchyComponent>()) {
                const auto& hier = entity.getComponent<HierarchyComponent>();
                if (hier.parent_uuid.isValid() && in_subtree.find(hier.parent_uuid) == in_subtree.end()) {
                    for (auto& component_res : entity_res.m_native_components) {
                        if (component_res.m_type_name != "HierarchyComponent") {
                            continue;
                        }
                        Json component_json;
                        if (ParseJsonText(component_res.m_component, component_json)) {
                            component_json["parent_uuid"] = 0;
                            component_res.m_component = component_json.dump();
                        }
                    }
                }
            }

            scene_res.m_entities.push_back(std::move(entity_res));
        }

        return scene_res;
    }

    void Scene::deserialize(const SceneRes& scene_res) {
        DO_PROFILE_SCOPE_CATEGORY("Scene::deserialize", "startup");
        const DynamicArray<Entity> entities = getEntities();
        for (Entity entity : entities) {
            destroyEntity(entity);
        }

        setName(scene_res.m_name);

        for (const auto& entity_res : scene_res.m_entities) {
            Entity entity = createEntity(entity_res.m_uuid, entity_res.m_name);
            DeserializeNativeComponents(entity_res.m_native_components, entity);
            DeserializeManagedComponents(entity_res.m_managed_components, entity);
        }

        const DynamicArray<Entity> loaded_entities = getEntities();
        for (Entity entity : loaded_entities) {
            if (!entity.hasComponent<HierarchyComponent>()) continue;
            auto& hier = entity.getComponent<HierarchyComponent>();
            hier.parent = Entity::NullEntity();
            hier.children.clear();
            if (!hier.parent_uuid.isValid()) continue;

            Entity parent = tryGetEntityByUUID(hier.parent_uuid);
            if (!parent.valid()) {
                hier.parent_uuid = UUID{};
                continue;
            }

            hier.parent = parent;
            if (parent.hasComponent<HierarchyComponent>()) {
                auto& parent_hier = parent.getComponent<HierarchyComponent>();
                if (std::find(parent_hier.children.begin(), parent_hier.children.end(), entity) == parent_hier.children.end()) {
                    parent_hier.children.push_back(entity);
                }
                parent_hier.child_count = static_cast<int>(parent_hier.children.size());
            }
        }

        DynamicArray<Entity> prefab_markers;
        for (Entity entity : loaded_entities) {
            if (entity.valid() && entity.hasComponent<PrefabInstanceComponent>()) {
                prefab_markers.push_back(entity);
            }
        }
        for (Entity marker : prefab_markers) {
            const auto inst = marker.getComponent<PrefabInstanceComponent>();

            Entity root = SceneImporter::InstantiatePrefab(inst.prefab);
            if (!root.valid()) continue;

            if (root.hasComponent<TransformComponent>()) {
                auto& tc = root.getComponent<TransformComponent>();
                tc.position = inst.position;
                tc.rotation = inst.rotation;
                tc.scale = inst.scale;
                tc.dirty = true;
            }

            destroyEntity(marker);
        }
    }

    Entity Scene::createEntity(const String& name) {
        return createEntity(UUID(), name);
    }

    Entity Scene::createEntity(UUID uuid, const String& name) {
        auto entity = m_reg.create();
        auto id = entity.addComponent<IDComponent>(uuid, name);
        id.name = name.empty() ? "Entity" : name;
        entity.addComponent<TagComponent>();
        entity.addComponent<TransformComponent>();

        m_entity_umap[uuid] = entity;

        return entity;
    }

    void Scene::addEntity(Entity entity) {
        if (!entity.hasComponent<IDComponent>()) {
            auto& id = entity.addComponent<IDComponent>();
            id.name = "Entity";
        }

        const auto& id = entity.getComponent<IDComponent>();
        if (m_entity_umap.find(id.id) != m_entity_umap.end()) {
            DO_ERROR("The scene already has the entity!");
            return;
        }

        m_entity_umap[id.id] = entity;
    }

    void Scene::destroyEntity(Entity entity) {
        if (auto* runtime = GetScriptRuntime()) {
            runtime->removeEntityFromManagedWorld(static_cast<UInt64>(entity.uuid()));
        }
        m_entity_umap.erase(entity.uuid());
        m_reg.destroy(entity);
    }

    Entity Scene::getEntityByTag(const String& tag) {
        for (auto entity : m_reg.view<TagComponent>()) {
            if (entity.getComponent<TagComponent>().tag == tag) {
                return entity;
            }
        }
        DO_ERROR("Not found entity has the tag {}.", tag);
        return Entity::NullEntity();
    }

    DynamicArray<Entity> Scene::getEntitiesByTag(const String& tag) {
        DynamicArray<Entity> result;
        for (auto entity : m_reg.view<TagComponent>()) {
            if (entity.getComponent<TagComponent>().tag == tag) {
                result.push_back(entity);
            }
        }
        return result;
    }

    Entity Scene::getEntity(const ui32 entity_id) {
        for (auto&[_, entity] : m_entity_umap) {
            if (static_cast<ui32>(entity) == entity_id) {
                return entity;
            }
        }
        DO_ERROR("Not found entity!");
        return Entity::NullEntity();
    }

    Entity Scene::tryGetEntityByUUID(const UUID uuid) const {
        if (auto it = m_entity_umap.find(uuid); it != m_entity_umap.end()) {
            return it->second;
        }
        return Entity();
    }

    Entity Scene::getEntityByUUID(const UUID uuid) {
        Entity entity = tryGetEntityByUUID(uuid);
        if (!entity.valid()) {
            DO_ERROR("Not found entity with UUID {}.", static_cast<uint64_t>(uuid));
        }
        return entity;
    }

    DynamicArray<Entity> Scene::getEntities() {
        DynamicArray<Entity> entities;
        entities.reserve(m_entity_umap.size());
        for (const auto& [_, entity] : m_entity_umap) {
            entities.push_back(entity);
        }
        return entities;
    }

} // dodoe
