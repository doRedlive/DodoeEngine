//
// Created by Redlive on 2026/3/23.
//

#ifndef DODOE_SYSTEMS_H
#define DODOE_SYSTEMS_H

#include "dopch.h"

#include "world.h"
#include "world_manager.h"
#include "registry.h"
#include "entity.h"
#include "components.h"
#include "systems/physics2d_system.h"

#include "runtime/resource/resource_manager.h"

namespace dodoe {

    namespace systems {

        inline system::Physics2dSystem& physics2d_system_instance(Registry& reg) {
            static std::unordered_map<Registry*, Scope<system::Physics2dSystem>> instances{};
            auto it = instances.find(&reg);
            if (it == instances.end()) {
                it = instances.emplace(&reg, create_scope<system::Physics2dSystem>()).first;
            }
            return *(it->second);
        }

        inline void Physics2dStartSystem(Registry& reg) {
            physics2d_system_instance(reg).start(reg);
        }

        inline void Physics2dUpdateSystem(Registry& reg, float dt) {
            if (dt <= 0.0f) {
                return;
            }
            auto& world = WorldManager::self().active_world();
            world.context.physics_system().step(dt);
            physics2d_system_instance(reg).update(reg);
        }

        // MARK: TODO: FIXME: establish communcation between the primary camera component and the camera instance. 
        inline void CameraUpdateSystem(Registry& reg, float dt) {
            (void)dt;
            auto& world = WorldManager::self().active_world();

            auto view = reg.view<CameraComponent, TagComponent, TransformComponent>();
            for (auto entity : view) {
                auto tag = reg.get<TagComponent>(entity).tag;
                if (tag != "Primary Camera") {
                    continue;
                }

                auto& camera = reg.get<CameraComponent>(entity);
                auto& transform = reg.get<TransformComponent>(entity);
                auto& camera_instance = world.context.camera();
                const bool component_changed = !camera.has_synced
                    || camera.last_synced_position.x != transform.position.x
                    || camera.last_synced_position.y != transform.position.y
                    || camera.last_synced_position.z != transform.position.z
                    || camera.last_synced_rotation != transform.rotation.z
                    || camera.last_synced_zoom != camera.zoom;
                if (component_changed) {
                    camera.dirty = true;
                }
                if (!camera.dirty) {
                    break;
                }

                camera_instance.set_position(transform.position);
                camera_instance.set_rotation(transform.rotation.z);
                camera_instance.set_zoom(camera.zoom);
                camera.last_synced_position = transform.position;
                camera.last_synced_rotation = transform.rotation.z;
                camera.last_synced_zoom = camera.zoom;
                camera.has_synced = true;
                camera.dirty = false;
                break;
            }
        }

        inline void SpriteRendererUpdateSystem(Registry& reg, float dt) {
            (void)dt;
            auto& world = WorldManager::self().active_world();
            auto& renderer = world.context.renderer();
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

        inline void Animation2dUpdateSystem(Registry& reg, float dt) {
            auto view = reg.view<Animation2dComponent, SpriteRendererComponent>();
            for (auto entity : view) {
                auto& anim2d = reg.get<Animation2dComponent>(entity);
                auto& sprite_renderer = reg.get<SpriteRendererComponent>(entity);

                auto it = anim2d.anim_clip_umap.find(anim2d.cur_anim_id);
                if (it == anim2d.anim_clip_umap.end()) {
                    continue;
                }
                const auto& current_animation = it->second;
                if (current_animation->frames.empty()) {
                    continue;
                }

                anim2d.cur_time_duration += dt * 1000.0f * anim2d.speed;

                if (anim2d.cur_frame_id >= current_animation->frames.size()) {
                    anim2d.cur_frame_id = 0;
                    anim2d.cur_time_duration = 0.0f;
                }

                const auto& frame = current_animation->frames[anim2d.cur_frame_id];
                if (anim2d.cur_time_duration >= frame.duration) {
                    anim2d.cur_time_duration -= frame.duration;
                    anim2d.cur_frame_id += 1;
                    if (anim2d.cur_frame_id >= current_animation->frames.size()) {
                        if (current_animation->loop) {
                            anim2d.cur_frame_id = 0;
                        } else {
                            anim2d.cur_frame_id = current_animation->frames.size() - 1;
                        }
                    }
                }

                sprite_renderer.texture_id = current_animation->frames[anim2d.cur_frame_id].texture_id;
            }
        }
    } // systems

} // dodoe

#endif//DODOE_SYSTEMS_H
