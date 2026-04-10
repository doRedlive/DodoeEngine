//
// Created by GreenMuffin on 2025/11/15.
//

#include "scene.h"

#include "world.h"
#include "entity.h"

#include "runtime/core/utils/common.h"
#include "runtime/function/world/components.h"

namespace dodoe {

    Scene::Scene(World& world, const std::string& name) : world_(world), name_(name), reg_(this) {
        auto entity = create_entity("Primary Camera"); 
        entity.add_component<CameraComponent>();
        entity.get_component<TagComponent>().set_tag("Primary Camera");
    }

    Scene::~Scene() = default;

    void Scene::on_runtime_start() {
        for (auto& on_start : world_.load_start_systems()) {
            on_start(reg_);
        }
    }

    void Scene::on_runtime_update(const float delta_time) {
        for (auto& on_update : world_.load_update_systems()) {
            on_update(reg_, delta_time);
        }
    }

    void Scene::on_runtime_stop() {

    }

    void Scene::on_simulation_start() {

    }

    void Scene::on_simulation_stop() {

    }

    void Scene::on_simulation_update(const float delta_time) {

    }

    void Scene::destroy() {
        on_runtime_stop();
        reg_.clear();
    }

    Entity Scene::create_entity(const std::string& name) {
        return create_entity(Uuid(), name);
    }

    Entity Scene::create_entity(Uuid uuid, const std::string& name) {
        auto entity = reg_.create();
        auto id = entity.add_component<IDComponent>(uuid, name);
        id.name = name.empty() ? "Entity" : name;
        entity.add_component<TagComponent>();
        entity.add_component<TransformComponent>();

        entity_umap_[uuid] = entity;

        return entity;
    }

    void Scene::add_entity(Entity entity) {
        if (entity.has_component<IDComponent>()) {
            auto id = entity.get_component<IDComponent>();
            if (entity_umap_.find(id.id) != entity_umap_.end()) {
                entity_umap_[id.id] = entity;
            }
            else {
                DoError("The scene already has the entity!");
            }
        }
        auto id = entity.add_component<IDComponent>();
        id.name = "Entity";

        entity_umap_[id.id] = entity.handle();
    }

    void Scene::destroy_entity(Entity entity) {
        entity_umap_.erase(entity.uuid());
        reg_.destroy(entity);
    }

    Entity Scene::get_entity(const std::string& tag) {
        for (auto& [_, entity] : entity_umap_) {
            if (entity.get_component<TagComponent>().id == String2Hash(tag)) {
                return entity;
            }
        }
        DoError("Not found entity has the tag {}.", tag);
        return Entity();
    }

} // dodoe
