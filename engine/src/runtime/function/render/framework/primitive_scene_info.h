// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/mesh_draw/mesh_batch.h"
#include "runtime/function/render/render_types.h"

namespace dodoe {

    class RenderObject;

    enum class PrimitiveMobility : UInt8 {
        Static,
        Stationary,
        Movable
    };

    class PrimitiveSceneInfo {
    public:
        struct Section {
            Ref<Material> material{};
            UInt32 index_offset{0};
            UInt32 vertex_offset{0};
            UInt32 index_count{0};
            UInt32 vertex_count{0};
            Int32 geometry_index{0};
            MeshGeometryPrimitiveType primitive_type{MeshGeometryPrimitiveType::Triangles};

            [[nodiscard]] Bool isDrawable() const { return index_count > 0; }
        };

    private:
        Identifier m_id{};
        const RenderObject* m_render_object{nullptr};
        Matrix4f m_world_transform{1.0f};
        Vector3f m_bounds_min{0.0f};
        Vector3f m_bounds_max{0.0f};
        Ref<Mesh> m_mesh{};
        DynamicArray<Ref<Material>> m_materials{};
        DynamicArray<Section> m_sections{};
        DynamicArray<MeshBatch> m_mesh_batches{};
        PrimitiveMobility m_mobility{PrimitiveMobility::Static};
        Bool m_visible{true};
        Bool m_cast_shadow{true};

    public:
        PrimitiveSceneInfo() = default;
        explicit PrimitiveSceneInfo(const Identifier id) : m_id(id) { }

        void setRenderObject(const RenderObject* render_object) { m_render_object = render_object; }
        void setWorldTransform(const Matrix4f& world_transform) { m_world_transform = world_transform; }
        void setMesh(const Ref<Mesh>& mesh) { m_mesh = mesh; }
        void setMaterials(const DynamicArray<Ref<Material>>& materials) { m_materials = materials; }
        void setSections(const DynamicArray<Section>& sections) { m_sections = sections; }
        void setMeshBatches(const DynamicArray<MeshBatch>& mesh_batches) { m_mesh_batches = mesh_batches; }
        void setMobility(const PrimitiveMobility mobility) { m_mobility = mobility; }
        void setVisible(const Bool visible) { m_visible = visible; }
        void setCastShadow(const Bool cast_shadow) { m_cast_shadow = cast_shadow; }
        void setBounds(const Vector3f& bounds_min, const Vector3f& bounds_max) {
            m_bounds_min = bounds_min;
            m_bounds_max = bounds_max;
        }

        [[nodiscard]] Identifier getId() const { return m_id; }
        [[nodiscard]] const RenderObject* getRenderObject() const { return m_render_object; }
        [[nodiscard]] const Matrix4f& getWorldTransform() const { return m_world_transform; }
        [[nodiscard]] const Vector3f& getBoundsMin() const { return m_bounds_min; }
        [[nodiscard]] const Vector3f& getBoundsMax() const { return m_bounds_max; }
        [[nodiscard]] const Ref<Mesh>& getMesh() const { return m_mesh; }
        [[nodiscard]] const DynamicArray<Ref<Material>>& getMaterials() const { return m_materials; }
        [[nodiscard]] const DynamicArray<Section>& getSections() const { return m_sections; }
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
        [[nodiscard]] const Ref<Material>& getMaterial(const Size_t material_index) const {
            static const Ref<Material> k_null_material{};
            return material_index < m_materials.size() ? m_materials[material_index] : k_null_material;
        }

    };

} // dodoe
