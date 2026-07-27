// do@Redlive

#include "sprite_renderer_system.h"

#include "runtime/core/context/system_context.h"
#include "runtime/function/render/render_command_queue.h"
#include "runtime/function/render/render_pipeline/renderer.h"
#include "runtime/function/render/render_scene/sprite_render_object.h"
#include "runtime/function/render/texture/texture_manager.h"

#include "runtime/core/math/math.h"

namespace dodoe {

    SpriteRendererSystem::~SpriteRendererSystem() = default;

    void SpriteRendererSystem::update(Registry& reg, float dt) {
        (void)dt;

        auto sprite_view = reg.view<IDComponent, TransformComponent, SpriteRendererComponent>();
        UnorderedSet<UUID> active_sprites{};

        for (auto entity : sprite_view) {
            auto& id = entity.getComponent<IDComponent>();
            auto& transform = entity.getComponent<TransformComponent>();
            auto& sr = entity.getComponent<SpriteRendererComponent>();
            active_sprites.insert(id.id);

            syncSpriteRenderer(entity);

            transform.dirty = false;
            id.dirty = false;
            sr.dirty = false;
        }

        pruneRemovedSprites(active_sprites);
    }

    bool SpriteRendererSystem::syncSpriteRenderer(Entity entity) {
        auto& id = entity.getComponent<IDComponent>();
        auto& transform = entity.getComponent<TransformComponent>();
        auto& sr = entity.getComponent<SpriteRendererComponent>();

        if (!needsSync(entity)) {
            return false;
        }

        auto* texture_manager = GetRenderSystem()->getSharedRenderService()->getTextureManager();

        Texture2D* texture = nullptr;
        const String& tex_path = sr.texture.getFileID().getPath();
        if (!tex_path.empty()) {
            texture = Texture2D::Load(tex_path);
        }

        if (!texture) {
            return false;
        }

        UInt32 flags = 0;
        if (sr.flip) {
            flags |= kSpriteFlagFlipX;
        }

        auto sprite_object = create_scope<SpriteRenderObject>();
        sprite_object->setUUID(id.id);
        sprite_object->setTexture(sr.texture);
        sprite_object->setColor(sr.color.to_rgba32());
        sprite_object->setFlags(flags);
        sprite_object->setUVRect(0.0f, 0.0f, 1.0f, 1.0f);
        sprite_object->setVisible(true);
        sprite_object->setWorldTransform(buildWorldMatrix(transform));

        RenderCommandQueue::AddSprite(std::move(sprite_object));
        m_submitted_sprites.insert(id.id);
        return true;
    }

    void SpriteRendererSystem::pruneRemovedSprites(const UnorderedSet<UUID>& active_sprites) {
        for (auto it = m_submitted_sprites.begin(); it != m_submitted_sprites.end();) {
            if (active_sprites.find(*it) == active_sprites.end()) {
                RenderCommandQueue::RemoveSprite(*it);
                it = m_submitted_sprites.erase(it);
                continue;
            }
            ++it;
        }
    }

    bool SpriteRendererSystem::needsSync(Entity entity) const {
        const auto& id = entity.getComponent<IDComponent>();
        const auto& transform = entity.getComponent<TransformComponent>();
        const auto& sr = entity.getComponent<SpriteRendererComponent>();

        return m_submitted_sprites.find(id.id) == m_submitted_sprites.end() ||
            transform.dirty ||
            id.dirty ||
            sr.dirty;
    }

    Matrix4f SpriteRendererSystem::buildWorldMatrix(const TransformComponent& transform) {
        Matrix4f world(1.0f);
        world = Math::Translate(world, Vector3f(transform.position.x, transform.position.y, 0.0f));
        world = Math::Rotate(world, Math::Radians(transform.rotation.z), Vector3f(0.0f, 0.0f, 1.0f));
        world = Math::Scale(world, Vector3f(transform.scale.x, transform.scale.y, 1.0f));
        return world;
    }

} // dodoe
