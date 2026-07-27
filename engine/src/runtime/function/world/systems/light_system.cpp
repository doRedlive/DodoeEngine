#include "light_system.h"

#include "runtime/core/context/system_context.h"
#include "runtime/function/render/render_command_queue.h"
#include "runtime/function/render/render_pipeline/renderer.h"
#include "runtime/function/render/render_scene/light_scene_info.h"

#include "runtime/core/math/math.h"

namespace dodoe {

    LightSystem::~LightSystem() = default;

    SystemAccess LightSystem::getAccess() const {
        return SystemAccessBuilder{}
            .readsComponents<IDComponent, TransformComponent, PointLightComponent, SpotLightComponent>()
            .build();
    }

    void LightSystem::update(Registry& reg, float dt) {
        (void)dt;

        UnorderedSet<UUID> active_lights{};

        auto point_view = reg.view<IDComponent, TransformComponent, PointLightComponent>();
        for (auto entity : point_view) {
            auto& id = entity.getComponent<IDComponent>();
            auto& light = entity.getComponent<PointLightComponent>();
            active_lights.insert(id.id);
            if (!light.enabled) {
                continue;
            }
            syncPointLight(entity);
        }

        auto spot_view = reg.view<IDComponent, TransformComponent, SpotLightComponent>();
        for (auto entity : spot_view) {
            auto& id = entity.getComponent<IDComponent>();
            auto& light = entity.getComponent<SpotLightComponent>();
            active_lights.insert(id.id);
            if (entity.hasComponent<PointLightComponent>() || !light.enabled) {
                continue;
            }
            syncSpotLight(entity);
        }

        pruneRemovedLights(active_lights);
    }

    bool LightSystem::syncPointLight(Entity entity) {
        auto& id = entity.getComponent<IDComponent>();
        auto& transform = entity.getComponent<TransformComponent>();
        auto& light = entity.getComponent<PointLightComponent>();

        if (!needsLightSync(entity, LightType::Point)) {
            return false;
        }

        LightSceneInfo info(static_cast<Identifier>(static_cast<uint64_t>(id.id)));
        info.setLightType(LightType::Point);
        info.setWorldTransform(buildWorldMatrix(transform));
        info.setEnabled(light.enabled);

        PointLightData data{};
        data.color = Vector3f(light.color.r, light.color.g, light.color.b);
        data.intensity = light.intensity;
        data.radius = light.radius;
        data.range = light.range;
        info.setPointLightData(data);

        RenderCommandQueue::AddLight(std::move(info));
        m_submitted_lights[id.id] = LightType::Point;

        transform.dirty = false;
        id.dirty = false;
        light.dirty = false;
        return true;
    }

    bool LightSystem::syncSpotLight(Entity entity) {
        auto& id = entity.getComponent<IDComponent>();
        auto& transform = entity.getComponent<TransformComponent>();
        auto& light = entity.getComponent<SpotLightComponent>();

        if (!needsLightSync(entity, LightType::Spot)) {
            return false;
        }

        LightSceneInfo info(static_cast<Identifier>(static_cast<uint64_t>(id.id)));
        info.setLightType(LightType::Spot);
        info.setWorldTransform(buildWorldMatrix(transform));
        info.setEnabled(light.enabled);

        SpotLightData data{};
        data.color = Vector3f(light.color.r, light.color.g, light.color.b);
        data.intensity = light.intensity;
        data.radius = light.radius;
        data.range = light.range;
        data.inner_angle = light.inner_angle;
        data.outer_angle = light.outer_angle;
        info.setSpotLightData(data);

        RenderCommandQueue::AddLight(std::move(info));
        m_submitted_lights[id.id] = LightType::Spot;

        transform.dirty = false;
        id.dirty = false;
        light.dirty = false;
        return true;
    }

    void LightSystem::pruneRemovedLights(const UnorderedSet<UUID>& active_lights) {
        for (auto it = m_submitted_lights.begin(); it != m_submitted_lights.end();) {
            if (!active_lights.contains(it->first)) {
                RenderCommandQueue::RemoveLight(it->first);
                it = m_submitted_lights.erase(it);
                continue;
            }
            ++it;
        }
    }

    bool LightSystem::needsLightSync(Entity entity, const LightType kind) const {
        const auto& id = entity.getComponent<IDComponent>();
        const auto& transform = entity.getComponent<TransformComponent>();
        const bool light_dirty = kind == LightType::Point
            ? entity.getComponent<PointLightComponent>().dirty
            : entity.getComponent<SpotLightComponent>().dirty;

        const auto submitted_it = m_submitted_lights.find(id.id);
        return submitted_it == m_submitted_lights.end() ||
            submitted_it->second != kind ||
            !GetRenderSystem()->getRenderScene()->hasLight(id.id) ||
            transform.dirty ||
            id.dirty ||
            light_dirty;
    }

    Matrix4f LightSystem::buildWorldMatrix(const TransformComponent& transform) {
        Matrix4f world(1.0f);
        world = Math::Translate(world, transform.position);
        world = Math::Rotate(world, Math::Radians(transform.rotation.x), Vector3f(1.0f, 0.0f, 0.0f));
        world = Math::Rotate(world, Math::Radians(transform.rotation.y), Vector3f(0.0f, 1.0f, 0.0f));
        world = Math::Rotate(world, Math::Radians(transform.rotation.z), Vector3f(0.0f, 0.0f, 1.0f));
        world = Math::Scale(world, transform.scale);
        return world;
    }

} // dodoe
