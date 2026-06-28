#include "mesh_renderer_system.h"

#include "runtime/core/context/system_context.h"
#include "runtime/function/render/renderer.h"
#include "runtime/function/render/render_scene/static_mesh_render_object.h"

#include "runtime/core/math/math.h"

namespace dodoe {

    MeshRendererSystem::~MeshRendererSystem() = default;

    void MeshRendererSystem::update(Registry& reg, float dt) {
        (void)dt;

        auto mesh_view = reg.view<IDComponent, TransformComponent, MeshRendererComponent>();
        std::unordered_set<UUID> active_renderers{};
        bool dirty = false;

        for (auto entity : mesh_view) {
            auto& id = entity.getComponent<IDComponent>();
            auto& transform = entity.getComponent<TransformComponent>();
            auto& mesh = entity.getComponent<MeshRendererComponent>();
            active_renderers.insert(id.id);

            dirty |= syncRenderObject(entity);

            transform.dirty = false;
            if (entity.hasComponent<HierarchyComponent>()) {
                entity.getComponent<HierarchyComponent>().dirty = false;
            }
            id.dirty = false;
            mesh.dirty = false;
        }

        pruneRemovedObjects(active_renderers);

        if (dirty) {
            GetRenderSystem()->getRenderScene()->flushUpdates();
        }
    }

    bool MeshRendererSystem::syncRenderObject(Entity entity) {
        auto& id = entity.getComponent<IDComponent>();
        auto& transform = entity.getComponent<TransformComponent>();
        auto& mesh = entity.getComponent<MeshRendererComponent>();

        if (!needsRenderObjectSync(entity, m_submitted_objects)) {
            return false;
        }

        if (mesh.lods.empty()) {
            Renderer::RemovePrimitive(id.id);
            m_submitted_objects.erase(id.id);
            return true;
        }

        auto render_object = buildRenderObject(mesh);
        render_object->setUUID(id.id);
        render_object->setWorldTransform(buildWorldMatrix(transform));
        Renderer::AddPrimitive(std::move(render_object));
        m_submitted_objects.insert(id.id);
        return true;
    }

    void MeshRendererSystem::pruneRemovedObjects(const std::unordered_set<UUID>& active_renderers) {
        for (auto it = m_submitted_objects.begin(); it != m_submitted_objects.end();) {
            if (active_renderers.find(*it) == active_renderers.end()) {
                Renderer::RemovePrimitive(*it);
                it = m_submitted_objects.erase(it);
                continue;
            }
            ++it;
        }
    }

    bool MeshRendererSystem::needsRenderObjectSync(Entity entity, const std::unordered_set<UUID>& submitted) {
        const auto& id = entity.getComponent<IDComponent>();
        const auto& transform = entity.getComponent<TransformComponent>();
        const auto& mesh = entity.getComponent<MeshRendererComponent>();
        const bool hierarchy_dirty = entity.hasComponent<HierarchyComponent>() && entity.getComponent<HierarchyComponent>().dirty;

        const auto* render_object = GetRenderSystem()->getRenderScene()->findPrimitive(id.id);
        return submitted.find(id.id) == submitted.end() ||
            render_object == nullptr ||
            render_object->getRenderObjectType() != RenderObjectType::StaticMesh ||
            render_object->getLODData().size() != mesh.lods.size() ||
            transform.dirty ||
            hierarchy_dirty ||
            id.dirty ||
            mesh.dirty;
    }

    Matrix4f MeshRendererSystem::buildWorldMatrix(const TransformComponent& transform) {
        Matrix4f world(1.0f);
        world = Math::Translate(world, transform.position);
        world = Math::Rotate(world, Math::Radians(transform.rotation.x), Vector3f(1.0f, 0.0f, 0.0f));
        world = Math::Rotate(world, Math::Radians(transform.rotation.y), Vector3f(0.0f, 1.0f, 0.0f));
        world = Math::Rotate(world, Math::Radians(transform.rotation.z), Vector3f(0.0f, 0.0f, 1.0f));
        world = Math::Scale(world, transform.scale);
        return world;
    }

    Scope<PrimitiveRenderObject> MeshRendererSystem::buildRenderObject(const MeshRendererComponent& mesh) {
        auto render_object = create_scope<StaticMeshRenderObject>();
        render_object->setUploadData(mesh.upload_data);
        render_object->setLODData(mesh.lods);
        render_object->setOverrideMaterials(mesh.override_materials);
        render_object->setMobility(mesh.mobility);
        render_object->setVisible(mesh.visible);
        render_object->setCastShadow(mesh.cast_shadow);
        return render_object;
    }

} // dodoe
