// do@Redlive

#pragma once

#include "dopch.h"

#include "system.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"
#include "runtime/function/render/renderer_2d.h"
#include "runtime/function/render/framework/texture_manager.h"
#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/render/render_system.h"

namespace dodoe {

    class SpriteRendererSystem : public System {
    public:
        ~SpriteRendererSystem() override = default;

        void update(Registry& reg, const float dt) override {
            (void)dt;
            reg.sort<SpriteRendererComponent>([](const SpriteRendererComponent& a, const SpriteRendererComponent& b) {
                return a.depth_ > b.depth_;
            });

            auto view = reg.view<SpriteRendererComponent, TransformComponent>();
            view.use<SpriteRendererComponent>();
            auto* texture_manager = Application::Self().context().getRenderSystem()->getTextureManager();
            if (!texture_manager) {
                return;
            }
            for (auto entity : view) {
                auto& sr = reg.get<SpriteRendererComponent>(entity);
                auto& tr = reg.get<TransformComponent>(entity);

                Ref<Texture> texture = nullptr;
                const String& tex_path = sr.texture.getFileID().getPath();
                if (!tex_path.empty()) {
                    texture = Texture::Load(tex_path);
                } else {
                    texture = texture_manager->getFallback();
                }
                if (!texture) {
                    continue;
                }

                constexpr float ppu = 10.0f;
                const Vector2f tex_size(
                    static_cast<float>(texture->getWidth()),
                    static_cast<float>(texture->getHeight())
                );
                const Vector2f world_size = (tex_size / ppu) * Vector2f(tr.scale.x, tr.scale.y);

                const Vector2f anchor_pos(tr.position.x, tr.position.y);
                const Vector2f pivot_offset = world_size * sr.pivot;
                const Vector2f bl_pos = anchor_pos - pivot_offset;

                Renderer2D::DrawSprite(
                    sr.texture,
                    bl_pos,
                    world_size,
                    tr.rotation,
                    sr.color
                );
            }
        }
    };

} // dodoe
