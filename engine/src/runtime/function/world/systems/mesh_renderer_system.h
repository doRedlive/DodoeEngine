// do@Redlive

#pragma once

#include "dopch.h"

#include "system.h"
#include "../components.h"
#include "runtime/function/render/render_resource.h"

namespace dodoe {

    class MeshRendererSystem : public System {
        std::unordered_set<Uuid> m_submitted_nodes;
    public:
        ~MeshRendererSystem() override = default;

        void update(Registry& reg, const float dt) override {
            (void)dt;

            auto& render_scene = g_RenderResource->getRenderScene();
            auto scene_graph = render_scene.getSceneGraph();
            std::unordered_set<Uuid> alive_nodes;
            bool dirty = false;

            auto node_view = reg.view<IDComponent, TransformComponent>();
            for (auto entity : node_view) {
                auto& id = entity.getComponent<IDComponent>();
                auto& transform = entity.getComponent<TransformComponent>();

                alive_nodes.insert(id.id);

                const bool node_missing = !scene_graph->hasNode(id.id);
                if (node_missing) {
                    m_submitted_nodes.erase(id.id);
                }

                const bool has_hierarchy = entity.hasComponent<HierarchyComponent>();
                const bool hierarchy_dirty = has_hierarchy && entity.getComponent<HierarchyComponent>().dirty;
                if (!m_submitted_nodes.contains(id.id) || node_missing || transform.dirty || hierarchy_dirty || id.dirty) {
                    submitNode(render_scene, entity);
                    dirty = true;
                }

                transform.dirty = false;
                if (has_hierarchy) {
                    entity.getComponent<HierarchyComponent>().dirty = false;
                }
                id.dirty = false;

                if (!entity.hasComponent<MeshRendererComponent>() && scene_graph->getMeshInstance(id.id)) {
                    render_scene.removeMeshInstance(id.id);
                    dirty = true;
                }
            }

            auto mesh_view = reg.view<IDComponent, MeshRendererComponent>();
            for (auto entity : mesh_view) {
                auto& id = entity.getComponent<IDComponent>();
                auto& mesh = entity.getComponent<MeshRendererComponent>();
                auto instance = scene_graph->getMeshInstance(id.id);
                if (!scene_graph->hasNode(id.id)) {
                    submitNode(render_scene, entity);
                    dirty = true;
                }

                if (!instance || instance->getMesh() != mesh.mesh || mesh.dirty) {
                    submitMesh(render_scene, entity);
                    dirty = true;
                }

                mesh.dirty = false;
            }

            for (auto it = m_submitted_nodes.begin(); it != m_submitted_nodes.end();) {
                if (!alive_nodes.contains(*it)) {
                    render_scene.removeNode(*it);
                    it = m_submitted_nodes.erase(it);
                    dirty = true;
                } else {
                    ++it;
                }
            }

            if (dirty) {
                render_scene.rebuild();
            }
        }
    private:
        void submitNode(RenderScene& render_scene, Entity entity) {
            auto& id = entity.getComponent<IDComponent>();
            auto& transform = entity.getComponent<TransformComponent>();

            Uuid parent_uuid{};
            if (entity.hasComponent<HierarchyComponent>()) {
                auto& hierarchy = entity.getComponent<HierarchyComponent>();
                if (hierarchy.parent.valid()) {
                    parent_uuid = hierarchy.parent.getComponent<IDComponent>().id;
                }
            }

            render_scene.upsertNode(
                id.id,
                id.name,
                parent_uuid,
                transform.position,
                transform.rotation,
                transform.scale
            );
            m_submitted_nodes.insert(id.id);
        }

        void submitMesh(RenderScene& render_scene, Entity entity) {
            auto& id = entity.getComponent<IDComponent>();
            auto& mesh = entity.getComponent<MeshRendererComponent>();
            if (!mesh.mesh) {
                render_scene.removeMeshInstance(id.id);
                return;
            }
            render_scene.upsertMeshInstance(id.id, mesh.mesh);
        }
    };

} // dodoe
