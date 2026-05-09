// do@Redlive
#pragma once

#include "dopch.h"

#include "system.h"
#include "../components.h"
#include "runtime/function/render/render_resource.h"

namespace dodoe {

    class LightSystem : public System {
        enum class LightKind {
            Point,
            Spot,
        };

        std::unordered_map<Uuid, LightKind> m_submitted_lights;

    public:
        ~LightSystem() override = default;

        void update(Registry& reg, const float dt) override {
            (void)dt;

            if (!g_RenderResource) {
                return;
            }

            auto& render_scene = g_RenderResource->getRenderScene();
            const auto scene_graph = render_scene.getSceneGraph();
            if (!scene_graph) {
                return;
            }

            std::unordered_set<Uuid> alive_nodes;
            bool dirty = false;

                auto submit_point_light = [&](Entity entity, auto& light) {
                auto& id = entity.getComponent<IDComponent>();
                auto& transform = entity.getComponent<TransformComponent>();
                alive_nodes.insert(id.id);

                Uuid parent_uuid{};
                if (entity.hasComponent<HierarchyComponent>()) {
                    auto& hierarchy = entity.getComponent<HierarchyComponent>();
                    if (hierarchy.parent.valid()) {
                        parent_uuid = hierarchy.parent.getComponent<IDComponent>().id;
                    }
                }

                    const bool node_missing = !scene_graph->hasNode(id.id);
                    const auto submitted_it = m_submitted_lights.find(id.id);
                    if (submitted_it == m_submitted_lights.end() || submitted_it->second != LightKind::Point || node_missing || transform.dirty || id.dirty || light.dirty) {
                    auto point_light = create_ref<PointLight>();
                    point_light->color = light.color;
                    point_light->intensity = light.intensity;
                    point_light->radius = light.radius;
                    point_light->range = light.range;

                    render_scene.upsertPointLight(id.id, id.name, parent_uuid, transform.position, transform.rotation, transform.scale, point_light);
                        m_submitted_lights[id.id] = LightKind::Point;
                    dirty = true;
                }

                transform.dirty = false;
                id.dirty = false;
                light.dirty = false;
            };

                auto submit_spot_light = [&](Entity entity, auto& light) {
                auto& id = entity.getComponent<IDComponent>();
                auto& transform = entity.getComponent<TransformComponent>();
                alive_nodes.insert(id.id);

                Uuid parent_uuid{};
                if (entity.hasComponent<HierarchyComponent>()) {
                    auto& hierarchy = entity.getComponent<HierarchyComponent>();
                    if (hierarchy.parent.valid()) {
                        parent_uuid = hierarchy.parent.getComponent<IDComponent>().id;
                    }
                }

                    const bool node_missing = !scene_graph->hasNode(id.id);
                    const auto submitted_it = m_submitted_lights.find(id.id);
                    if (submitted_it == m_submitted_lights.end() || submitted_it->second != LightKind::Spot || node_missing || transform.dirty || id.dirty || light.dirty) {
                    auto spot_light = create_ref<SpotLight>();
                    spot_light->color = light.color;
                    spot_light->intensity = light.intensity;
                    spot_light->radius = light.radius;
                    spot_light->range = light.range;
                    spot_light->inner_angle = light.inner_angle;
                    spot_light->outer_angle = light.outer_angle;

                    render_scene.upsertSpotLight(id.id, id.name, parent_uuid, transform.position, transform.rotation, transform.scale, spot_light);
                        m_submitted_lights[id.id] = LightKind::Spot;
                    dirty = true;
                }

                transform.dirty = false;
                id.dirty = false;
                light.dirty = false;
            };

            auto point_view = reg.view<IDComponent, TransformComponent, PointLightComponent>();
            for (auto entity : point_view) {
                auto& light = entity.getComponent<PointLightComponent>();
                if (!light.enabled) {
                    continue;
                }
                submit_point_light(entity, light);
            }

            auto spot_view = reg.view<IDComponent, TransformComponent, SpotLightComponent>();
            for (auto entity : spot_view) {
                if (entity.hasComponent<PointLightComponent>()) {
                    continue;
                }
                auto& light = entity.getComponent<SpotLightComponent>();
                if (!light.enabled) {
                    continue;
                }
                submit_spot_light(entity, light);
            }

                for (auto it = m_submitted_lights.begin(); it != m_submitted_lights.end();) {
                    if (!alive_nodes.contains(it->first)) {
                        render_scene.removeNode(it->first);
                        it = m_submitted_lights.erase(it);
                    dirty = true;
                } else {
                    ++it;
                }
            }

            if (dirty) {
                render_scene.rebuild();
            }
        }

    };

} // dodoe