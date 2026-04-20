//
// Created by GreenMuffin on 2025/11/15.
//

#include "scene.h"

#include "world.h"
#include "entity.h"

#include "runtime/core/utils/common.h"
#include "runtime/function/world/components.h"

namespace dodoe {

    Scene::Scene(World& world, const std::string& name) : m_world(world), m_name(name), m_reg(this) { }

    Scope<Scene> Scene::create(const SceneCreateInfo& info) {
        auto scene = create_scope<Scene>(info.world, info.name);
        if (!scene->initialize()) return nullptr;
        return scene;
    }

    void Scene::destroy(Scope<Scene>& scene) {
        if (!scene) return;
        scene->shutdown();
        scene.reset();
    }

    void Scene::save() {

    }

    void Scene::onCreate() {

    }

    void Scene::onDelete() {

    }

    bool Scene::initialize() {
        auto entity = createEntity("Primary Camera"); 
        auto& camera = entity.addComponent<Camera2dComponent>();
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

    Entity Scene::createEntity(const std::string& name) {
        return createEntity(Uuid(), name);
    }

    Entity Scene::createEntity(Uuid uuid, const std::string& name) {
        auto entity = m_reg.create();
        auto id = entity.addComponent<IDComponent>(uuid, name);
        id.name = name.empty() ? "Entity" : name;
        entity.addComponent<TagComponent>();
        entity.addComponent<TransformComponent>();

        m_entity_umap[uuid] = entity;

        return entity;
    }

    void Scene::addEntity(Entity entity) {
        if (entity.hasComponent<IDComponent>()) {
            auto id = entity.getComponent<IDComponent>();
            if (m_entity_umap.find(id.id) != m_entity_umap.end()) {
                m_entity_umap[id.id] = entity;
            }
            else {
                DO_ERROR("The scene already has the entity!");
            }
        }
        auto id = entity.addComponent<IDComponent>();
        id.name = "Entity";

        m_entity_umap[id.id] = entity.handle();
    }

    void Scene::destroyEntity(Entity entity) {
        m_entity_umap.erase(entity.uuid());
        m_reg.destroy(entity);
    }

    Entity Scene::getEntityByTag(const std::string& tag) {
        for (auto& [_, entity] : m_entity_umap) {
            if (entity.getComponent<TagComponent>().id == string2hash(tag)) {
                return entity;
            }
        }
        DO_ERROR("Not found entity has the tag {}.", tag);
        return Entity::nullEntity();
    }

    Entity Scene::getEntity(const ui32 entity_id) {
        for (auto&[_, entity] : m_entity_umap) {
            if (static_cast<ui32>(entity) == entity_id) {
                return entity;
            }
        }
        DO_ERROR("Not found entity!");
        return Entity::nullEntity();
    }

    Entity Scene::getEntityByUUID(Uuid uuid) {
        if (m_entity_umap.find(uuid) != m_entity_umap.end()) {
            return m_entity_umap[uuid];
        }
        DO_ERROR("Not found entity with UUID {}.", static_cast<uint64_t>(uuid));
        return Entity();
    }

    std::vector<Entity> Scene::getEntities() {
        std::vector<Entity> entities;
        entities.reserve(m_entity_umap.size());
        for (const auto& [_, entity] : m_entity_umap) {
            entities.push_back(entity);
        }
        return entities;
    }

} // dodoe
