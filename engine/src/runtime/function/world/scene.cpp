//
// Created by GreenMuffin on 2025/11/15.
//

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

        std::vector<ComponentRes> SerializeNativeComponents(Entity entity) {
            std::vector<ComponentRes> components;
            auto& component_db = ComponentDB::self();
            for (const auto& entry : component_db.entries()) {
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

    } // namespace

    Scene::Scene(const SceneCreateInfo& info) : Scene(info.world, info.name) { }

    Scene::Scene(World& world, const String& name) : m_world(world), m_name(name), m_reg(this) { }

    void Scene::save() {
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
        m_world.onSimulationUpdate(m_reg, dt);
    }

    SceneRes Scene::serialize() const {
        SceneRes scene_res;
        scene_res.m_name = getName();

        for (Entity entity : const_cast<Scene*>(this)->getEntities()) {
            EntityRes entity_res;
            entity_res.m_uuid = entity.uuid();
            entity_res.m_name = entity.name();
            entity_res.m_native_components = SerializeNativeComponents(entity);
            entity_res.m_managed_components = SerializeManagedComponents(entity);
            scene_res.m_entities.push_back(std::move(entity_res));
        }

        return scene_res;
    }

    void Scene::deserialize(const SceneRes& scene_res) {
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
