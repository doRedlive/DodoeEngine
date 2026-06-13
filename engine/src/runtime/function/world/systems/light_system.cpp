#include "light_system.h"

#include "render_system_bridge.h"

namespace dodoe {

    LightSystem::~LightSystem() = default;

    void LightSystem::update(Registry& reg, float dt) {
        (void)dt;

        const RenderSceneSyncScope render_sync = TryBeginRenderSceneSync();
        if (!render_sync) {
            return;
        }

        auto& render_scene = render_sync.scene();
        std::unordered_set<Uuid> alive_nodes{};
        bool dirty = false;

        auto point_view = reg.view<IDComponent, TransformComponent, PointLightComponent>();
        for (auto entity : point_view) {
            auto& id = entity.getComponent<IDComponent>();
            auto& light = entity.getComponent<PointLightComponent>();
            alive_nodes.insert(id.id);
            if (!light.enabled) {
                continue;
            }
            dirty |= syncPointLight(render_scene, entity, light);
        }

        auto spot_view = reg.view<IDComponent, TransformComponent, SpotLightComponent>();
        for (auto entity : spot_view) {
            auto& id = entity.getComponent<IDComponent>();
            auto& light = entity.getComponent<SpotLightComponent>();
            alive_nodes.insert(id.id);
            if (entity.hasComponent<PointLightComponent>() || !light.enabled) {
                continue;
            }
            dirty |= syncSpotLight(render_scene, entity, light);
        }

        dirty |= pruneRemovedLights(render_scene, alive_nodes);

        render_sync.flushIfDirty(dirty);
    }

    bool LightSystem::syncPointLight(RenderScene& render_scene, Entity entity, PointLightComponent& light) {
        auto& id = entity.getComponent<IDComponent>();
        auto& transform = entity.getComponent<TransformComponent>();
        const bool dirty = needsLightSync(render_scene, entity, LightKind::Point, light.dirty);

        if (dirty) {
            RenderLightObject point_light{};
            point_light.type = RenderLightType::Point;
            point_light.color = light.color;
            point_light.intensity = light.intensity;
            point_light.radius = light.radius;
            point_light.range = light.range;
            render_scene.upsertPointLight(
                id.id,
                id.name,
                resolveParentUuid(entity),
                transform.position,
                transform.rotation,
                transform.scale,
                point_light
            );
            m_submitted_lights[id.id] = LightKind::Point;
        }

        transform.dirty = false;
        id.dirty = false;
        light.dirty = false;
        return dirty;
    }

    bool LightSystem::syncSpotLight(RenderScene& render_scene, Entity entity, SpotLightComponent& light) {
        auto& id = entity.getComponent<IDComponent>();
        auto& transform = entity.getComponent<TransformComponent>();
        const bool dirty = needsLightSync(render_scene, entity, LightKind::Spot, light.dirty);

        if (dirty) {
            RenderLightObject spot_light{};
            spot_light.type = RenderLightType::Spot;
            spot_light.color = light.color;
            spot_light.intensity = light.intensity;
            spot_light.radius = light.radius;
            spot_light.range = light.range;
            spot_light.inner_angle = light.inner_angle;
            spot_light.outer_angle = light.outer_angle;
            render_scene.upsertSpotLight(
                id.id,
                id.name,
                resolveParentUuid(entity),
                transform.position,
                transform.rotation,
                transform.scale,
                spot_light
            );
            m_submitted_lights[id.id] = LightKind::Spot;
        }

        transform.dirty = false;
        id.dirty = false;
        light.dirty = false;
        return dirty;
    }

    bool LightSystem::pruneRemovedLights(RenderScene& render_scene, const std::unordered_set<Uuid>& alive_nodes) {
        bool dirty = false;
        for (auto it = m_submitted_lights.begin(); it != m_submitted_lights.end();) {
            if (!alive_nodes.contains(it->first)) {
                render_scene.removeLightObject(it->first);
                it = m_submitted_lights.erase(it);
                dirty = true;
                continue;
            }
            ++it;
        }
        return dirty;
    }

    bool LightSystem::needsLightSync(const RenderScene& render_scene, Entity entity, LightKind kind, const bool light_dirty) const {
        const auto& id = entity.getComponent<IDComponent>();
        const auto& transform = entity.getComponent<TransformComponent>();
        const bool node_missing = !render_scene.hasNode(id.id);
        const auto submitted_it = m_submitted_lights.find(id.id);
        return submitted_it == m_submitted_lights.end() ||
            submitted_it->second != kind ||
            node_missing ||
            transform.dirty ||
            id.dirty ||
            light_dirty;
    }

    Uuid LightSystem::resolveParentUuid(Entity entity) {
        if (!entity.hasComponent<HierarchyComponent>()) {
            return {};
        }

        auto& hierarchy = entity.getComponent<HierarchyComponent>();
        if (!hierarchy.parent.valid()) {
            return {};
        }

        return hierarchy.parent.getComponent<IDComponent>().id;
    }

} // dodoe
