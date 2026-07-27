// do@Redlive

#include "line_renderer_system.h"

#include "runtime/core/context/system_context.h"
#include "runtime/function/render/render_command_queue.h"
#include "runtime/function/render/render_pipeline/renderer.h"
#include "runtime/function/render/render_scene/sprite_render_object.h"

#include "runtime/core/math/math.h"

namespace dodoe {

    LineRendererSystem::~LineRendererSystem() = default;

    SystemAccess LineRendererSystem::getAccess() const {
        return SystemAccessBuilder{}
            .readsComponents<IDComponent, TransformComponent, LineRendererComponent>()
            .build();
    }

    void LineRendererSystem::update(Registry& reg, Float dt) {
        (void)dt;

        auto view = reg.view<IDComponent, TransformComponent, LineRendererComponent>();
        UnorderedSet<UUID> active{};

        for (auto entity : view) {
            auto& id = entity.getComponent<IDComponent>();
            active.insert(id.id);
            syncLine(entity);

            auto& t = entity.getComponent<TransformComponent>();
            auto& lc = entity.getComponent<LineRendererComponent>();
            t.dirty = false;
            id.dirty = false;
            lc.dirty = false;
        }

        pruneRemoved(active);
    }

    Bool LineRendererSystem::syncLine(Entity entity) {
        auto& id = entity.getComponent<IDComponent>();
        auto& transform = entity.getComponent<TransformComponent>();
        auto& line = entity.getComponent<LineRendererComponent>();

        if (!needsSync(entity)) {
            return false;
        }

        auto sprite_object = create_scope<SpriteRenderObject>();
        sprite_object->setUUID(id.id);
        sprite_object->setColor(line.color.to_rgba32());
        sprite_object->setUVRect(0.0f, 0.0f, 1.0f, 1.0f);
        sprite_object->setVisible(true);
        sprite_object->setWorldTransform(buildWorldMatrix(transform, line));

        RenderCommandQueue::AddSprite(std::move(sprite_object));
        m_submitted.insert(id.id);
        return true;
    }

    void LineRendererSystem::pruneRemoved(const UnorderedSet<UUID>& active) {
        for (auto it = m_submitted.begin(); it != m_submitted.end();) {
            if (active.find(*it) == active.end()) {
                RenderCommandQueue::RemoveSprite(*it);
                it = m_submitted.erase(it);
                continue;
            }
            ++it;
        }
    }

    Bool LineRendererSystem::needsSync(Entity entity) const {
        const auto& id = entity.getComponent<IDComponent>();
        const auto& transform = entity.getComponent<TransformComponent>();
        const auto& line = entity.getComponent<LineRendererComponent>();

        return m_submitted.find(id.id) == m_submitted.end() ||
            transform.dirty ||
            id.dirty ||
            line.dirty;
    }

    Matrix4f LineRendererSystem::buildWorldMatrix(const TransformComponent& transform, const LineRendererComponent& line) const {
        const Float angle = std::atan2(line.direction.y, line.direction.x);
        const Float half_len = line.length * 0.5f;

        Matrix4f world(1.0f);
        world = Math::Translate(world, Vector3f(transform.position.x, transform.position.y, 0.0f));
        world = Math::Rotate(world, angle, Vector3f(0.0f, 0.0f, 1.0f));
        world = Math::Translate(world, Vector3f(half_len, 0.0f, 0.0f));
        world = Math::Scale(world, Vector3f(line.length, line.thickness, 1.0f));
        return world;
    }

} // dodoe
