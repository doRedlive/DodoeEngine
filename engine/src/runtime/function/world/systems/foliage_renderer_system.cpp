#include "foliage_renderer_system.h"

#include "render_system_bridge.h"
#include "runtime/function/render/foliage_render_object.h"

namespace dodoe {

    FoliageRendererSystem::~FoliageRendererSystem() = default;

    void FoliageRendererSystem::update(Registry& reg, float dt) {
        (void)dt;

        const RenderSceneSyncScope render_sync = TryBeginRenderSceneSync();
        if (!render_sync) {
            return;
        }

        auto& render_scene = render_sync.scene();
        std::unordered_set<Uuid> alive_objects{};
        bool dirty = false;

        auto foliage_view = reg.view<IDComponent, TransformComponent, FoliageRendererComponent>();
        for (auto entity : foliage_view) {
            auto& id = entity.getComponent<IDComponent>();
            auto& transform = entity.getComponent<TransformComponent>();
            auto& foliage = entity.getComponent<FoliageRendererComponent>();
            alive_objects.insert(id.id);

            dirty |= syncFoliageRenderer(render_scene, entity);

            transform.dirty = false;
            if (entity.hasComponent<HierarchyComponent>()) {
                entity.getComponent<HierarchyComponent>().dirty = false;
            }
            id.dirty = false;
            foliage.dirty = false;
        }

        dirty |= pruneRemovedObjects(render_scene, alive_objects);
        render_sync.flushIfDirty(dirty);
    }

    bool FoliageRendererSystem::syncFoliageRenderer(RenderScene& render_scene, Entity entity) {
        if (!needsObjectSync(render_scene, entity)) {
            return false;
        }

        auto& id = entity.getComponent<IDComponent>();
        auto& foliage = entity.getComponent<FoliageRendererComponent>();
        render_scene.upsertRenderObject(id.id, buildRenderObject(foliage));
        m_submitted_objects.insert(id.id);
        return true;
    }

    bool FoliageRendererSystem::pruneRemovedObjects(RenderScene& render_scene, const std::unordered_set<Uuid>& alive_objects) {
        bool dirty = false;
        for (auto it = m_submitted_objects.begin(); it != m_submitted_objects.end();) {
            if (!alive_objects.contains(*it)) {
                render_scene.removeRenderObject(*it);
                it = m_submitted_objects.erase(it);
                dirty = true;
                continue;
            }
            ++it;
        }
        return dirty;
    }

    bool FoliageRendererSystem::needsObjectSync(const RenderScene& render_scene, Entity entity) const {
        const auto& id = entity.getComponent<IDComponent>();
        const auto& transform = entity.getComponent<TransformComponent>();
        const auto& foliage = entity.getComponent<FoliageRendererComponent>();
        const bool hierarchy_dirty = entity.hasComponent<HierarchyComponent>() && entity.getComponent<HierarchyComponent>().dirty;
        const auto* render_object = render_scene.findRenderObject(id.id);
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
            foliage_type.mesh = component.mesh;
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

    Scope<RenderObject> FoliageRendererSystem::buildRenderObject(const FoliageRendererComponent& component) {
        auto render_object = create_scope<FoliageRenderObject>();
        render_object->applyType(buildFoliageType(component));
        render_object->setInstances(buildInstanceData(component));
        return render_object;
    }

} // dodoe
