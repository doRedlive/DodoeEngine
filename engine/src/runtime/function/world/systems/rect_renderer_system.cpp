// do@Redlive

#include "rect_renderer_system.h"

#include "runtime/core/context/system_context.h"
#include "runtime/function/render/renderer.h"
#include "runtime/function/render/render_scene/sprite_render_object.h"

#include "runtime/core/math/math.h"

namespace dodoe {

    RectRendererSystem::~RectRendererSystem() = default;

    void RectRendererSystem::update(Registry& reg, Float dt) {
        (void)dt;

        auto view = reg.view<IDComponent, TransformComponent, RectRendererComponent>();
        std::unordered_set<UUID> active{};
        Bool dirty = false;

        for (auto entity : view) {
            auto& id = entity.getComponent<IDComponent>();
            active.insert(id.id);
            dirty |= syncRect(entity);

            auto& t = entity.getComponent<TransformComponent>();
            auto& rc = entity.getComponent<RectRendererComponent>();
            t.dirty = false;
            id.dirty = false;
            rc.dirty = false;
        }

        pruneRemoved(active);

        if (dirty) {
            GetRenderSystem()->getRenderScene()->flushUpdates();
        }
    }

    Bool RectRendererSystem::syncRect(Entity entity) {
        auto& id = entity.getComponent<IDComponent>();
        auto& transform = entity.getComponent<TransformComponent>();
        auto& rect = entity.getComponent<RectRendererComponent>();

        if (!needsSync(entity)) {
            return false;
        }

        auto sprite_object = create_scope<SpriteRenderObject>();
        sprite_object->setUUID(id.id);
        sprite_object->setColor(rect.color.to_rgba32());
        sprite_object->setUVRect(0.0f, 0.0f, 1.0f, 1.0f);
        sprite_object->setVisible(true);
        sprite_object->setWorldTransform(buildWorldMatrix(transform, rect));

        RenderCommandQueue::AddSprite(std::move(sprite_object));
        m_submitted.insert(id.id);
        return true;
    }

    void RectRendererSystem::pruneRemoved(const std::unordered_set<UUID>& active) {
        for (auto it = m_submitted.begin(); it != m_submitted.end();) {
            if (active.find(*it) == active.end()) {
                RenderCommandQueue::RemoveSprite(*it);
                it = m_submitted.erase(it);
                continue;
            }
            ++it;
        }
    }

    Bool RectRendererSystem::needsSync(Entity entity) const {
        const auto& id = entity.getComponent<IDComponent>();
        const auto& transform = entity.getComponent<TransformComponent>();
        const auto& rect = entity.getComponent<RectRendererComponent>();

        return m_submitted.find(id.id) == m_submitted.end() ||
            transform.dirty ||
            id.dirty ||
            rect.dirty;
    }

    Matrix4f RectRendererSystem::buildWorldMatrix(const TransformComponent& transform, const RectRendererComponent& rect) const {
        Matrix4f world(1.0f);
        world = Math::Translate(world, Vector3f(transform.position.x, transform.position.y, 0.0f));
        world = Math::Rotate(world, Math::Radians(transform.rotation.z), Vector3f(0.0f, 0.0f, 1.0f));
        world = Math::Scale(world, Vector3f(rect.size.x, rect.size.y, 1.0f));
        return world;
    }

} // dodoe
