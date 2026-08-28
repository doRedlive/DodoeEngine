#include "mesh_renderer_system.h"

#include "runtime/core/context/system_context.h"
#include "runtime/function/render/render_command_queue.h"
#include "runtime/function/render/render_pipeline/renderer.h"
#include "runtime/function/render/render_scene/static_mesh_render_object.h"
#include "runtime/function/render/mesh_draw/mesh.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/resource/file/file_id.h"

#include "runtime/core/math/math.h"

namespace dodoe {

    MeshRendererSystem::~MeshRendererSystem() = default;

    SystemAccess MeshRendererSystem::getAccess() const {
        return SystemAccessBuilder{}
            .readsComponents<IDComponent, TransformComponent, MeshRendererComponent, HierarchyComponent, AnimationPoseComponent>()
            .build();
    }

    void MeshRendererSystem::update(Registry& reg, float dt) {
        (void)dt;
        if (!GetRenderSystem()) { return; }

        auto mesh_view = reg.view<IDComponent, TransformComponent, MeshRendererComponent>();
        UnorderedSet<UUID> active_renderers{};

        for (auto entity : mesh_view) {
            auto& id = entity.getComponent<IDComponent>();
            auto& transform = entity.getComponent<TransformComponent>();
            auto& mesh = entity.getComponent<MeshRendererComponent>();
            active_renderers.insert(id.id);

            syncRenderObject(entity);

            transform.dirty = false;
            if (entity.hasComponent<HierarchyComponent>()) {
                entity.getComponent<HierarchyComponent>().dirty = false;
            }
            id.dirty = false;
            mesh.dirty = false;
            if (entity.hasComponent<AnimationPoseComponent>()) {
                auto& pose = entity.getComponent<AnimationPoseComponent>();
                if (pose.dirty) {
                    mesh.skinning_matrices = pose.skinning_matrices;
                    pose.dirty = false;
                    mesh.dirty = true;
                }
            }
        }

        pruneRemovedObjects(active_renderers);
    }

    bool MeshRendererSystem::syncRenderObject(Entity entity) {
        auto& id = entity.getComponent<IDComponent>();
        auto& transform = entity.getComponent<TransformComponent>();
        auto& mesh = entity.getComponent<MeshRendererComponent>();

        if (!needsRenderObjectSync(entity, m_submitted_objects)) {
            return false;
        }

        Mesh* resolved = mesh.mesh.get();
        if (!resolved && mesh.mesh.getObjectID().isValid()) {
            const ObjectID& ref = mesh.mesh.getObjectID();
            resolved = ResourceManager::Self().loadObject<Mesh>(ref.asset_id, ref.local_id);
        }
        if (!resolved && !mesh.mesh.getLegacyPath().empty()) {
            resolved = ResourceManager::Self().loadObjectByPath<Mesh>(FileID(mesh.mesh.getLegacyPath()));
        }
        if (!resolved) {
            RenderCommandQueue::RemovePrimitive(id.id);
            m_submitted_objects.erase(id.id);
            return true;
        }

        const String legacy_path = mesh.mesh.getLegacyPath();
        mesh.mesh = PPtr<Mesh>(resolved);
        if (!legacy_path.empty()) {
            mesh.mesh.setLegacyPath(legacy_path);
        }

        auto render_object = buildRenderObject(mesh);
        render_object->setUUID(id.id);
        render_object->setWorldTransform(buildWorldMatrix(transform));
        RenderCommandQueue::AddPrimitive(std::move(render_object));
        m_submitted_objects.insert(id.id);
        return true;
    }

    void MeshRendererSystem::pruneRemovedObjects(const UnorderedSet<UUID>& active_renderers) {
        for (auto it = m_submitted_objects.begin(); it != m_submitted_objects.end();) {
            if (active_renderers.find(*it) == active_renderers.end()) {
                RenderCommandQueue::RemovePrimitive(*it);
                it = m_submitted_objects.erase(it);
                continue;
            }
            ++it;
        }
    }

    bool MeshRendererSystem::needsRenderObjectSync(Entity entity, const UnorderedSet<UUID>& submitted) {
        const auto& id = entity.getComponent<IDComponent>();
        const auto& transform = entity.getComponent<TransformComponent>();
        const auto& mesh = entity.getComponent<MeshRendererComponent>();
        const bool hierarchy_dirty = entity.hasComponent<HierarchyComponent>() && entity.getComponent<HierarchyComponent>().dirty;

        const auto* render_object = GetRenderSystem()->getRenderScene()->findPrimitive(id.id);
        return submitted.find(id.id) == submitted.end() ||
            render_object == nullptr ||
            render_object->getRenderObjectType() != RenderObjectType::StaticMesh ||
            render_object->getMesh() != mesh.mesh.get() ||
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
        render_object->setMesh(mesh.mesh.get(), mesh.section_index);
        render_object->setOverrideMaterials(mesh.override_materials);
        render_object->setMobility(mesh.mobility);
        render_object->setVisible(mesh.visible);
        render_object->setCastShadow(mesh.cast_shadow);
        return render_object;
    }

} // dodoe
