#include "light_system.h"

#include "runtime/function/render/renderer.h"
#include "runtime/function/render/render_scene/light_render_object.h"

#include <glm/gtc/matrix_transform.hpp>

namespace dodoe {

    LightSystem::~LightSystem() = default;

    void LightSystem::update(Registry& reg, float dt) {
        (void)dt;

        std::unordered_set<UUID> active_lights{};
        bool dirty = false;

        auto point_view = reg.view<IDComponent, TransformComponent, PointLightComponent>();
        for (auto entity : point_view) {
            auto& id = entity.getComponent<IDComponent>();
            auto& light = entity.getComponent<PointLightComponent>();
            active_lights.insert(id.id);
            if (!light.enabled) {
                continue;
            }
            dirty |= syncPointLight(entity);
        }

        auto spot_view = reg.view<IDComponent, TransformComponent, SpotLightComponent>();
        for (auto entity : spot_view) {
            auto& id = entity.getComponent<IDComponent>();
            auto& light = entity.getComponent<SpotLightComponent>();
            active_lights.insert(id.id);
            if (entity.hasComponent<PointLightComponent>() || !light.enabled) {
                continue;
            }
            dirty |= syncSpotLight(entity);
        }

        pruneRemovedLights(active_lights);

        if (dirty) {
            Renderer::FlushSceneUpdates();
        }
    }

    bool LightSystem::syncPointLight(Entity entity) {
        auto& id = entity.getComponent<IDComponent>();
        auto& transform = entity.getComponent<TransformComponent>();
        auto& light = entity.getComponent<PointLightComponent>();

        if (!needsLightSync(entity, LightKind::Point)) {
            return false;
        }

        auto light_object = create_scope<PointLightRenderObject>();
        light_object->setColor(light.color);
        light_object->setIntensity(light.intensity);
        light_object->setRadius(light.radius);
        light_object->setRange(light.range);
        light_object->setUUID(id.id);
        light_object->setWorldTransform(buildWorldMatrix(transform));

        Renderer::AddLight(std::move(light_object));
        m_submitted_lights[id.id] = LightKind::Point;

        transform.dirty = false;
        id.dirty = false;
        light.dirty = false;
        return true;
    }

    bool LightSystem::syncSpotLight(Entity entity) {
        auto& id = entity.getComponent<IDComponent>();
        auto& transform = entity.getComponent<TransformComponent>();
        auto& light = entity.getComponent<SpotLightComponent>();

        if (!needsLightSync(entity, LightKind::Spot)) {
            return false;
        }

        auto light_object = create_scope<SpotLightRenderObject>();
        light_object->setColor(light.color);
        light_object->setIntensity(light.intensity);
        light_object->setRadius(light.radius);
        light_object->setRange(light.range);
        light_object->setInnerAngle(light.inner_angle);
        light_object->setOuterAngle(light.outer_angle);
        light_object->setUUID(id.id);
        light_object->setWorldTransform(buildWorldMatrix(transform));

        Renderer::AddLight(std::move(light_object));
        m_submitted_lights[id.id] = LightKind::Spot;

        transform.dirty = false;
        id.dirty = false;
        light.dirty = false;
        return true;
    }

    void LightSystem::pruneRemovedLights(const std::unordered_set<UUID>& active_lights) {
        for (auto it = m_submitted_lights.begin(); it != m_submitted_lights.end();) {
            if (!active_lights.contains(it->first)) {
                Renderer::RemoveLight(it->first);
                it = m_submitted_lights.erase(it);
                continue;
            }
            ++it;
        }
    }

    bool LightSystem::needsLightSync(Entity entity, const LightKind kind) const {
        const auto& id = entity.getComponent<IDComponent>();
        const auto& transform = entity.getComponent<TransformComponent>();
        const bool light_dirty = kind == LightKind::Point
            ? entity.getComponent<PointLightComponent>().dirty
            : entity.getComponent<SpotLightComponent>().dirty;

        const auto submitted_it = m_submitted_lights.find(id.id);
        return submitted_it == m_submitted_lights.end() ||
            submitted_it->second != kind ||
            !Renderer::HasLight(id.id) ||
            transform.dirty ||
            id.dirty ||
            light_dirty;
    }

    Matrix4f LightSystem::buildWorldMatrix(const TransformComponent& transform) {
        Matrix4f world(1.0f);
        world = glm::translate(world, transform.position);
        world = glm::rotate(world, glm::radians(transform.rotation.x), Vector3f(1.0f, 0.0f, 0.0f));
        world = glm::rotate(world, glm::radians(transform.rotation.y), Vector3f(0.0f, 1.0f, 0.0f));
        world = glm::rotate(world, glm::radians(transform.rotation.z), Vector3f(0.0f, 0.0f, 1.0f));
        world = glm::scale(world, transform.scale);
        return world;
    }

} // dodoe
