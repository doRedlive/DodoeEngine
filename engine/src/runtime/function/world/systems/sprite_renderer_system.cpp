#include "sprite_renderer_system.h"

#include "render_system_bridge.h"

namespace dodoe {

    SpriteRendererSystem::~SpriteRendererSystem() = default;

    void SpriteRendererSystem::update(Registry& reg, float dt) {
        (void)dt;

        reg.sort<SpriteRendererComponent>([](const SpriteRendererComponent& a, const SpriteRendererComponent& b) {
            return a.depth_ > b.depth_;
        });

        auto view = reg.view<SpriteRendererComponent, TransformComponent>();
        view.use<SpriteRendererComponent>();

        auto* texture_manager = TryGetTextureManager();
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

} // dodoe
