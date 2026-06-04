#ifndef DODOE_ANIMATION2D_SYSTEM_H
#define DODOE_ANIMATION2D_SYSTEM_H

#include "dopch.h"

#include "system.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"

namespace dodoe {

    class Animation2dSystem : public System {
    public:
        ~Animation2dSystem() override = default;

        void update(Registry& reg, const float dt) override {
            auto view = reg.view<Animation2dComponent, SpriteRendererComponent>();
            for (auto entity : view) {
                auto& anim2d = reg.get<Animation2dComponent>(entity);
                auto& sprite_renderer = reg.get<SpriteRendererComponent>(entity);

                auto it = anim2d.anim_clip_umap.find(anim2d.cur_anim_id);
                if (it == anim2d.anim_clip_umap.end()) {
                    continue;
                }
                const auto& current_animation = it->second;
                if (!current_animation || current_animation->frames.empty()) {
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

                const auto& current_frame = current_animation->frames[anim2d.cur_frame_id];
                if (current_frame.texture_id != 0) {
                    auto* tex = static_cast<Texture*>(Object::FindObjectFromInstanceID(current_frame.texture_id));
                    if (tex) {
                        sprite_renderer.texture = PPtr<Texture>(tex->getFileID(), tex->getUUID(), current_frame.texture_id);
                    }
                }
            }
        }
    };

} // dodoe

#endif//DODOE_ANIMATION2D_SYSTEM_H
