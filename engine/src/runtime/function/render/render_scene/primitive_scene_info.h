// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/mesh_draw/mesh_batch.h"
#include "runtime/function/render/mesh_draw/mesh_data.h"
#include "runtime/function/render/mesh_draw/mesh_draw_types.h"

namespace dodoe {

    enum class PrimitiveMobility : UInt8 {
        Static,
        Stationary,
        Movable
    };

    class PrimitiveSceneInfo {
    private:
        Identifier m_id{};
        Matrix4f m_world_transform{1.0f};
        Vector3f m_bounds_min{0.0f};
        Vector3f m_bounds_max{0.0f};
        DynamicArray<Ref<Material>> m_materials{};
        DynamicArray<SubMesh> m_sub_meshes{};
        DynamicArray<MeshBatch> m_mesh_batches{};
        PrimitiveMobility m_mobility{PrimitiveMobility::Static};
        Bool m_visible{true};
        Bool m_cast_shadow{true};
#ifdef DODOE_EDITOR_ENABLED
        Bool m_editor_only{false};
#endif
        UInt32 m_instance_count{1};
        DynamicArray<InstanceSceneData> m_instance_scene_data{};

    public:
        PrimitiveSceneInfo() = default;
        explicit PrimitiveSceneInfo(const Identifier id) : m_id(id) { }

        void setWorldTransform(const Matrix4f& world_transform) { m_world_transform = world_transform; }
        void setMaterials(const DynamicArray<Ref<Material>>& materials) { m_materials = materials; }
        void setSubMeshes(const DynamicArray<SubMesh>& sub_meshes) { m_sub_meshes = sub_meshes; }
        void setMeshBatches(const DynamicArray<MeshBatch>& mesh_batches) { m_mesh_batches = mesh_batches; }
        void setMobility(const PrimitiveMobility mobility) { m_mobility = mobility; }
        void setVisible(const Bool visible) { m_visible = visible; }
        void setCastShadow(const Bool cast_shadow) { m_cast_shadow = cast_shadow; }
        void setBounds(const Vector3f& bounds_min, const Vector3f& bounds_max) {
            m_bounds_min = bounds_min;
            m_bounds_max = bounds_max;
        }
        void setInstanceCount(const UInt32 count) { m_instance_count = count; }
        void setInstanceSceneData(const DynamicArray<InstanceSceneData>& data) { m_instance_scene_data = data; }

        [[nodiscard]] Identifier getId() const { return m_id; }
        [[nodiscard]] const Matrix4f& getWorldTransform() const { return m_world_transform; }
        [[nodiscard]] const Vector3f& getBoundsMin() const { return m_bounds_min; }
        [[nodiscard]] const Vector3f& getBoundsMax() const { return m_bounds_max; }
        [[nodiscard]] const DynamicArray<Ref<Material>>& getMaterials() const { return m_materials; }
        [[nodiscard]] const DynamicArray<SubMesh>& getSubMeshes() const { return m_sub_meshes; }
        [[nodiscard]] DynamicArray<MeshBatch>& getMeshBatches() { return m_mesh_batches; }
        [[nodiscard]] const DynamicArray<MeshBatch>& getMeshBatches() const { return m_mesh_batches; }
        [[nodiscard]] Bool hasRelevantBatch(const MeshPassType pass_type) const {
            for (const auto& batch : m_mesh_batches) {
                if (batch.isValid() && batch.isRelevant(pass_type)) {
                    return true;
                }
            }
            return false;
        }
        [[nodiscard]] PrimitiveMobility getMobility() const { return m_mobility; }
        [[nodiscard]] Bool isVisible() const { return m_visible; }
        [[nodiscard]] Bool castsShadow() const { return m_cast_shadow; }
#ifdef DODOE_EDITOR_ENABLED
        void setEditorOnly(Bool v) { m_editor_only = v; }
#endif
        [[nodiscard]] Bool isEditorOnly() const {
#ifdef DODOE_EDITOR_ENABLED
            return m_editor_only;
#else
            return false;
#endif
        }
        [[nodiscard]] const Ref<Material>& getMaterial(const Size_t material_index) const {
            static const Ref<Material> k_null_material{};
            return material_index < m_materials.size() ? m_materials[material_index] : k_null_material;
        }
        [[nodiscard]] UInt32 getInstanceCount() const { return m_instance_count; }
        [[nodiscard]] const DynamicArray<InstanceSceneData>& getInstanceSceneData() const { return m_instance_scene_data; }

    };

} // dodoe
