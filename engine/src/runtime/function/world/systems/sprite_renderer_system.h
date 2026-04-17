#ifndef DODOE_SPRITE_RENDERER_SYSTEM_H
#define DODOE_SPRITE_RENDERER_SYSTEM_H

#include "dopch.h"

#include "system.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"
#include "runtime/function/render/renderer_2d.h"
#include "runtime/function/render/framework/texture_manager.h"

namespace dodoe {

    class SpriteRendererSystem : public System {
    public:
        ~SpriteRendererSystem() override = default;

        void update(Registry& reg, float dt) override {
            (void)dt;
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

                const auto texture = TextureManager::self().loadTexture(sr.texture_id);
                if (!texture) {
                    continue;
                }

                constexpr float ppu = 10.0f;
                const Vector2f tex_size(
                    static_cast<float>(texture->width),
                    static_cast<float>(texture->height)
                );
                const Vector2f world_size = (tex_size / ppu) * Vector2f(tr.scale.x, tr.scale.y);

                const Vector2f anchor_pos(tr.position.x, tr.position.y);
                const Vector2f pivot_offset = world_size * sr.pivot;
                const Vector2f bl_pos = anchor_pos - pivot_offset;

                Renderer2d::drawSprite(
                    sr.texture_id,
                    bl_pos,
                    world_size,
                    tr.rotation,
                    sr.color
                );
            }
        }
    };

} // dodoe

#endif//DODOE_SPRITE_RENDERER_SYSTEM_H
