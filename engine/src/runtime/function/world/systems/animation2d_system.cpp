#include "animation2d_system.h"

#include "runtime/service/sprite/sprite_loader.h"

namespace dodoe {

    Animation2dSystem::~Animation2dSystem() = default;

    SystemAccess Animation2dSystem::getAccess() const {
        return SystemAccessBuilder{}
            .readsComponents<Animation2dComponent, SpriteRendererComponent>()
            .writesComponents<Animation2dComponent, SpriteRendererComponent>()
            .build();
    }

    void Animation2dSystem::update(Registry& reg, float dt) {
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
                    anim2d.cur_frame_id = current_animation->loop ? 0 : current_animation->frames.size() - 1;
                }
            }

            const auto& current_frame = current_animation->frames[anim2d.cur_frame_id];
            if (current_frame.texture_id == 0) {
                continue;
            }

            auto* tex = static_cast<Texture2D*>(Object::FindObjectFromInstanceID(current_frame.texture_id));
            if (tex) {
                sprite_renderer.sprite = PPtr<Sprite>(SpriteLoader::Load(tex->getPath()));
                sprite_renderer.sprite.setLegacyPath(tex->getPath());
            }
        }
    }

} // dodoe
