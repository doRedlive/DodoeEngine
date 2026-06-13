#include "mesh_renderer_system.h"

#include "render_system_bridge.h"
#include "runtime/function/render/static_mesh_render_object.h"

namespace dodoe {

    MeshRendererSystem::~MeshRendererSystem() = default;

    void MeshRendererSystem::update(Registry& reg, float dt) {
        (void)dt;

        const RenderSceneSyncScope render_sync = TryBeginRenderSceneSync();
        if (!render_sync) {
            return;
        }

        auto& render_scene = render_sync.scene();
        std::unordered_set<Uuid> alive_nodes{};
        bool dirty = false;

        auto node_view = reg.view<IDComponent, TransformComponent>();
        for (auto entity : node_view) {
            auto& id = entity.getComponent<IDComponent>();
            alive_nodes.insert(id.id);

            dirty |= syncNode(render_scene, entity);
            dirty |= removeDetachedRenderObject(render_scene, entity);

            auto& transform = entity.getComponent<TransformComponent>();
            transform.dirty = false;

            if (entity.hasComponent<HierarchyComponent>()) {
                entity.getComponent<HierarchyComponent>().dirty = false;
            }
            id.dirty = false;
        }

        auto mesh_view = reg.view<IDComponent, MeshRendererComponent>();
        for (auto entity : mesh_view) {
            auto& mesh = entity.getComponent<MeshRendererComponent>();
            dirty |= syncRenderObject(render_scene, entity);
            mesh.dirty = false;
        }

        dirty |= pruneRemovedNodes(render_scene, alive_nodes);

        render_sync.flushIfDirty(dirty);
    }

    bool MeshRendererSystem::syncNode(RenderScene& render_scene, Entity entity) {
        if (!needsNodeSync(render_scene, entity, m_submitted_nodes)) {
            return false;
        }

        auto& id = entity.getComponent<IDComponent>();
        auto& transform = entity.getComponent<TransformComponent>();
        render_scene.upsertNode(
            id.id,
            id.name,
            resolveParentUuid(entity),
            transform.position,
            transform.rotation,
            transform.scale
        );
        m_submitted_nodes.insert(id.id);
        return true;
    }

    bool MeshRendererSystem::syncRenderObject(RenderScene& render_scene, Entity entity) {
        auto& id = entity.getComponent<IDComponent>();
        auto& mesh = entity.getComponent<MeshRendererComponent>();

        if (!render_scene.hasNode(id.id)) {
            syncNode(render_scene, entity);
        }

        if (!needsRenderObjectSync(render_scene, entity)) {
            return false;
        }

        if (!mesh.mesh) {
            render_scene.removeRenderObject(id.id);
            return true;
        }

        render_scene.upsertRenderObject(id.id, buildRenderObject(mesh));
        return true;
    }

    bool MeshRendererSystem::removeDetachedRenderObject(RenderScene& render_scene, Entity entity) {
        auto& id = entity.getComponent<IDComponent>();
        if (entity.hasComponent<MeshRendererComponent>() || entity.hasComponent<FoliageRendererComponent>() || !render_scene.hasRenderObject(id.id)) {
            return false;
        }

        render_scene.removeRenderObject(id.id);
        return true;
    }

    bool MeshRendererSystem::pruneRemovedNodes(RenderScene& render_scene, const std::unordered_set<Uuid>& alive_nodes) {
        bool dirty = false;
        for (auto it = m_submitted_nodes.begin(); it != m_submitted_nodes.end();) {
            if (!alive_nodes.contains(*it)) {
                render_scene.removeNode(*it);
                it = m_submitted_nodes.erase(it);
                dirty = true;
                continue;
            }
            ++it;
        }
        return dirty;
    }

    bool MeshRendererSystem::needsNodeSync(
        const RenderScene& render_scene,
        Entity entity,
        const std::unordered_set<Uuid>& submitted_nodes)
    {
        const auto& id = entity.getComponent<IDComponent>();
        const auto& transform = entity.getComponent<TransformComponent>();
        const bool has_hierarchy = entity.hasComponent<HierarchyComponent>();
        const bool hierarchy_dirty = has_hierarchy && entity.getComponent<HierarchyComponent>().dirty;
        return !submitted_nodes.contains(id.id) ||
            !render_scene.hasNode(id.id) ||
            transform.dirty ||
            hierarchy_dirty ||
            id.dirty;
    }

    bool MeshRendererSystem::needsRenderObjectSync(const RenderScene& render_scene, Entity entity) {
        const auto& id = entity.getComponent<IDComponent>();
        const auto& mesh = entity.getComponent<MeshRendererComponent>();
        const auto* render_object = render_scene.findRenderObject(id.id);
        return render_object == nullptr ||
            render_object->getRenderObjectType() != RenderObjectType::StaticMesh ||
            render_object->getMesh() != mesh.mesh ||
            mesh.dirty;
    }

    Uuid MeshRendererSystem::resolveParentUuid(Entity entity) {
        if (!entity.hasComponent<HierarchyComponent>()) {
            return {};
        }

        auto& hierarchy = entity.getComponent<HierarchyComponent>();
        if (!hierarchy.parent.valid()) {
            return {};
        }

        return hierarchy.parent.getComponent<IDComponent>().id;
    }

    Scope<RenderObject> MeshRendererSystem::buildRenderObject(const MeshRendererComponent& mesh) {
        auto render_object = create_scope<StaticMeshRenderObject>();
        render_object->setMesh(mesh.mesh);
        render_object->setOverrideMaterials(mesh.override_materials);
        render_object->setMobility(mesh.mobility);
        render_object->setVisible(mesh.visible);
        render_object->setCastShadow(mesh.cast_shadow);
        return render_object;
    }

} // dodoe
