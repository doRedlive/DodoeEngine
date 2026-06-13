#pragma once

#include "dopch.h"

#include "render_object.h"

namespace dodoe {

    struct FoliageRenderType {
        Ref<Mesh> mesh{};
        DynamicArray<Ref<Material>> override_materials{};
        PrimitiveMobility mobility{PrimitiveMobility::Static};
        Bool visible{true};
        Bool cast_shadow{true};
        Vector3f instance_bounds_extent{0.5f, 0.5f, 0.5f};
    };

    struct FoliageRenderInstanceData {
        Vector3f position{0.0f, 0.0f, 0.0f};
        Vector3f rotation{0.0f, 0.0f, 0.0f};
        Vector3f scale{1.0f, 1.0f, 1.0f};
        Vector4f color_tint{1.0f, 1.0f, 1.0f, 1.0f};
        Float wind_phase{0.0f};
        Float variation{0.0f};
    };

    class FoliageRenderObject final : public RenderObject {
    public:
        struct Cluster {
            UInt32 first_instance{0};
            UInt32 instance_count{0};
            Vector3f bounds_min{0.0f};
            Vector3f bounds_max{0.0f};
        };

    private:
        Vector3f m_instance_bounds_extent{0.5f, 0.5f, 0.5f};
        Vector2f m_cluster_grid_size{8.0f, 8.0f};
        DynamicArray<FoliageRenderInstanceData> m_instances{};
        DynamicArray<Cluster> m_clusters{};

    public:
        void applyType(const FoliageRenderType& type);
        void setInstanceBoundsExtent(const Vector3f& instance_bounds_extent);
        void setInstances(const DynamicArray<FoliageRenderInstanceData>& instances);

        [[nodiscard]] const Vector3f& getInstanceBoundsExtent() const { return m_instance_bounds_extent; }
        [[nodiscard]] const DynamicArray<FoliageRenderInstanceData>& getInstances() const { return m_instances; }
        [[nodiscard]] const DynamicArray<Cluster>& getClusters() const { return m_clusters; }

        [[nodiscard]] RenderObjectType getRenderObjectType() const override { return RenderObjectType::Foliage; }
        [[nodiscard]] UInt32 getInstanceCount() const override;
        void appendInstanceSceneData(DynamicArray<InstanceSceneData>& out_instance_scene_data, const Matrix4f& world_transform) const override;
        [[nodiscard]] RenderObjectDirtyFlags diff(const RenderObject& previous) const override;
        [[nodiscard]] DynamicArray<MeshBatch> buildMeshBatches(
            Identifier primitive_id,
            const DynamicArray<Ref<Material>>& resolved_materials,
            UInt32 first_instance) const override;
        [[nodiscard]] PrimitiveSceneInfo buildSceneInfo(
            Identifier primitive_id,
            const Matrix4f& world_transform,
            const Vector3f& bounds_min,
            const Vector3f& bounds_max) const override;

    private:
        [[nodiscard]] Matrix4f buildInstanceWorldTransform(const FoliageRenderInstanceData& instance, const Matrix4f& world_transform) const;
        void computeLocalBounds(Vector3f& out_bounds_min, Vector3f& out_bounds_max) const;
        void rebuildClusters();
    };

} // dodoe
