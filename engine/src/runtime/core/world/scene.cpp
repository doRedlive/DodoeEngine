//
// Created by GreenMuffin on 2025/11/15.
//

#include "scene.h"

#include "world.h"
#include "entity.h"
#include "world_manager.h"

#include "runtime/core/utils/common.h"
#include "runtime/core/world/components.h"

#include "runtime/function/render/render_system.h"
#include "runtime/function/render/camera/camera.h"
#include "runtime/function/window/window_manager.h"

#include "runtime/resource/resource_manager.h"

namespace dodoe {

    namespace systems {
        using namespace dodoe;

        void SpriteRendererSystem(Registry& reg, float dt) {
            (void)dt;
            auto& world = WorldManager::self().active_world();

            auto& renderer = world.context.renderer;
            reg.sort<SpriteRendererComponent>([](const SpriteRendererComponent& a, const SpriteRendererComponent& b) {
                return a.depth_ > b.depth_;
            });

            auto view = reg.view<SpriteRendererComponent, TransformComponent>();
            view.use<SpriteRendererComponent>();
            for (auto entity : view) {
                auto& sr = reg.get<SpriteRendererComponent>(entity);
                auto& tr = reg.get<TransformComponent>(entity);

                if (sr.texture_id == 0) {
                    continue;
                }

                const auto texture_res = ResourceManager::self().get_texture(sr.texture_id);
                if (!texture_res.texture) {
                    continue;
                }

                const float ppu = (texture_res.ppu > 0.0f) ? texture_res.ppu : 10.0f;
                const Vector2f tex_size(static_cast<float>(texture_res.texture->width), static_cast<float>(texture_res.texture->height));
                const Vector2f world_size = (tex_size / ppu) * Vector2f(tr.scale.x, tr.scale.y);

                const Vector2f anchor_pos(tr.position.x, tr.position.y);
                const Vector2f pivot_offset = world_size * sr.pivot;
                const Vector2f bl_pos = anchor_pos - pivot_offset;

                renderer.draw_sprite(texture_res.texture, bl_pos, world_size, tr.rotation,
                    {sr.color.r, sr.color.g, sr.color.b, sr.color.a});
            }
        }
    }

    Scene::Scene(World& world, const std::string& name) : world_(world), name_(name), reg_(this), registry_(reg_) {
        world.add_update_system(systems::SpriteRendererSystem);
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

        entity_umap_[uuid] = entity.handle();

        return entity;
    }

    void Scene::add_entity(Entity entity) {
        if (entity.has_component<IDComponent>()) {
            auto id = entity.get_component<IDComponent>();
            if (entity_umap_.find(id.id) != entity_umap_.end()) {
                entity_umap_[id.id] = entity.handle();
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

} // dodoe
