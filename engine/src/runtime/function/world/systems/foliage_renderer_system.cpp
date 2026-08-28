#include "foliage_renderer_system.h"

#include "runtime/core/context/system_context.h"
#include "runtime/function/render/render_command_queue.h"
#include "runtime/function/render/render_pipeline/renderer.h"
#include "runtime/function/render/render_scene/foliage_render_object.h"
#include "runtime/function/render/mesh_draw/mesh.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/resource/file/file_id.h"

#include "runtime/core/math/math.h"

namespace dodoe {

    FoliageRendererSystem::~FoliageRendererSystem() = default;

    SystemAccess FoliageRendererSystem::getAccess() const {
        return SystemAccessBuilder{}
            .readsComponents<IDComponent, TransformComponent, FoliageRendererComponent, HierarchyComponent>()
            .build();
    }

    void FoliageRendererSystem::update(Registry& reg, float dt) {
        (void)dt;
        if (!GetRenderSystem()) { return; }

        auto foliage_view = reg.view<IDComponent, TransformComponent, FoliageRendererComponent>();
        UnorderedSet<UUID> active_objects{};

        for (auto entity : foliage_view) {
            auto& id = entity.getComponent<IDComponent>();
            auto& transform = entity.getComponent<TransformComponent>();
            auto& foliage = entity.getComponent<FoliageRendererComponent>();
            active_objects.insert(id.id);

            syncFoliageRenderer(entity);

            transform.dirty = false;
            if (entity.hasComponent<HierarchyComponent>()) {
                entity.getComponent<HierarchyComponent>().dirty = false;
            }
            id.dirty = false;
            foliage.dirty = false;
        }

        pruneRemovedObjects(active_objects);
    }

    bool FoliageRendererSystem::syncFoliageRenderer(Entity entity) {
        auto& id = entity.getComponent<IDComponent>();
        auto& transform = entity.getComponent<TransformComponent>();
        auto& foliage = entity.getComponent<FoliageRendererComponent>();

        if (!needsObjectSync(entity)) {
            return false;
        }

        Mesh* resolved = foliage.mesh.get();
        if (!resolved && foliage.mesh.getObjectID().isValid()) {
            const ObjectID& ref = foliage.mesh.getObjectID();
            resolved = ResourceManager::Self().loadObject<Mesh>(ref.asset_id, ref.local_id);
        }
        if (!resolved && !foliage.mesh.getLegacyPath().empty()) {
            resolved = ResourceManager::Self().loadObjectByPath<Mesh>(FileID(foliage.mesh.getLegacyPath()));
        }
        if (!resolved) {
            RenderCommandQueue::RemovePrimitive(id.id);
            m_submitted_objects.erase(id.id);
            return true;
        }

        const String legacy_path = foliage.mesh.getLegacyPath();
        foliage.mesh = PPtr<Mesh>(resolved);
        if (!legacy_path.empty()) {
            foliage.mesh.setLegacyPath(legacy_path);
        }

        auto render_object = buildRenderObject(foliage);
        render_object->setUUID(id.id);
        render_object->setWorldTransform(buildWorldMatrix(transform));
        RenderCommandQueue::AddPrimitive(std::move(render_object));
        m_submitted_objects.insert(id.id);
        return true;
    }

    void FoliageRendererSystem::pruneRemovedObjects(const UnorderedSet<UUID>& active_objects) {
        for (auto it = m_submitted_objects.begin(); it != m_submitted_objects.end();) {
            if (!active_objects.contains(*it)) {
                RenderCommandQueue::RemovePrimitive(*it);
                it = m_submitted_objects.erase(it);
                continue;
            }
            ++it;
        }
    }

    bool FoliageRendererSystem::needsObjectSync(Entity entity) const {
        const auto& id = entity.getComponent<IDComponent>();
        const auto& transform = entity.getComponent<TransformComponent>();
        const auto& foliage = entity.getComponent<FoliageRendererComponent>();
        const bool hierarchy_dirty = entity.hasComponent<HierarchyComponent>() && entity.getComponent<HierarchyComponent>().dirty;

        const auto* render_object = GetRenderSystem()->getRenderScene()->findPrimitive(id.id);
        return !m_submitted_objects.contains(id.id) ||
            render_object == nullptr ||
            render_object->getRenderObjectType() != RenderObjectType::Foliage ||
            transform.dirty ||
            hierarchy_dirty ||
            id.dirty ||
            foliage.dirty;
    }

    namespace {

        FoliageRenderType buildFoliageType(const FoliageRendererComponent& component) {
            FoliageRenderType foliage_type{};
            foliage_type.mesh = component.mesh.get();
            foliage_type.override_materials = component.override_materials;
            foliage_type.mobility = component.mobility;
            foliage_type.visible = component.visible;
            foliage_type.cast_shadow = component.cast_shadow;
            foliage_type.instance_bounds_extent = component.instance_bounds_extent;
            return foliage_type;
        }

        DynamicArray<FoliageRenderInstanceData> buildInstanceData(const FoliageRendererComponent& component) {
            DynamicArray<FoliageRenderInstanceData> instances{};
            instances.reserve(component.instances.size());
            for (const auto& instance : component.instances) {
                FoliageRenderInstanceData render_instance{};
                render_instance.position = instance.position;
                render_instance.rotation = instance.rotation;
                render_instance.scale = instance.scale;
                render_instance.color_tint = Vector4f(instance.color_tint.r, instance.color_tint.g, instance.color_tint.b, instance.color_tint.a);
                render_instance.wind_phase = instance.wind_phase;
                render_instance.variation = instance.variation;
                instances.push_back(render_instance);
            }
            return instances;
        }

    } // namespace

    Scope<PrimitiveRenderObject> FoliageRendererSystem::buildRenderObject(const FoliageRendererComponent& component) {
        auto render_object = create_scope<FoliageRenderObject>();
        render_object->applyType(buildFoliageType(component));
        render_object->setInstances(buildInstanceData(component));
        return render_object;
    }

    Matrix4f FoliageRendererSystem::buildWorldMatrix(const TransformComponent& transform) {
        Matrix4f world(1.0f);
        world = Math::Translate(world, transform.position);
        world = Math::Rotate(world, Math::Radians(transform.rotation.x), Vector3f(1.0f, 0.0f, 0.0f));
        world = Math::Rotate(world, Math::Radians(transform.rotation.y), Vector3f(0.0f, 1.0f, 0.0f));
        world = Math::Rotate(world, Math::Radians(transform.rotation.z), Vector3f(0.0f, 0.0f, 1.0f));
        world = Math::Scale(world, transform.scale);
        return world;
    }

} // dodoe
