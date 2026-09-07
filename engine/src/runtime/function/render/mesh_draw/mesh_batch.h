// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"
#include "mesh_pass_type.h"

namespace dodoe {

    class MaterialSystem;
    struct MaterialInstance;

    struct MeshBatchPassMask {
        Bool pass_relevance[static_cast<Size_t>(MeshPassType::Count)]{false};

        void setRelevant(const MeshPassType pass_type, const Bool relevant) {
            pass_relevance[static_cast<Size_t>(pass_type)] = relevant;
        }

        [[nodiscard]] Bool isRelevant(const MeshPassType pass_type) const {
            return pass_relevance[static_cast<Size_t>(pass_type)];
        }
    };

    struct MeshBatchElement {
        struct DrawRange {
            UInt32 index_count{0};
            UInt32 index_offset{0};
            UInt32 vertex_offset{0};
        };

        struct InstanceRange {
            Bool explicit_range{false};
            UInt32 first_instance{0};
            UInt32 instance_count{1};
        };

        UInt32 index_count{0};
        UInt32 index_offset{0};
        UInt32 vertex_offset{0};
        UInt32 section_index{0};
        Bool uses_instance_range{false};
        UInt32 first_instance{0};
        UInt32 instance_count{1};
        GfxBufferHandle vertex_buffer;
        GfxBufferHandle index_buffer;

        [[nodiscard]] Bool isValid() const {
            return index_count > 0 && instance_count > 0 && vertex_buffer && index_buffer;
        }
        [[nodiscard]] DrawRange getDrawRange() const {
            return DrawRange{index_count, index_offset, vertex_offset};
        }
        [[nodiscard]] InstanceRange getInstanceRange() const {
            return InstanceRange{uses_instance_range, first_instance, instance_count};
        }
        [[nodiscard]] UInt32 getSectionIndex() const { return section_index; }
    };

    class MeshBatch {
        Identifier m_primitive_id{};
        UInt32 m_material_index{0};
        MaterialInstance* m_material_instance{nullptr};
        MeshBatchPassMask m_pass_mask{};
        DynamicArray<MeshBatchElement> m_elements{};
        Bool m_uses_custom_bounds{false};
        Vector3f m_bounds_min{0.0f};
        Vector3f m_bounds_max{0.0f};

    public:
        void setPrimitiveId(const Identifier primitive_id) { m_primitive_id = primitive_id; }
        void setMaterialIndex(const UInt32 material_index) { m_material_index = material_index; }
        void setMaterialInstance(MaterialInstance* material_instance) { m_material_instance = material_instance; }
        void setRelevant(const MeshPassType pass_type, const Bool relevant) {
            m_pass_mask.setRelevant(pass_type, relevant);
        }
        void setCustomBounds(const Vector3f& bounds_min, const Vector3f& bounds_max) {
            m_uses_custom_bounds = true;
            m_bounds_min = bounds_min;
            m_bounds_max = bounds_max;
        }
        void addElement(MeshBatchElement element) { m_elements.push_back(std::move(element)); }

        [[nodiscard]] Identifier getPrimitiveId() const { return m_primitive_id; }
        [[nodiscard]] UInt32 getMaterialIndex() const { return m_material_index; }
        [[nodiscard]] MaterialInstance* getMaterialInstance() const { return m_material_instance; }
        [[nodiscard]] const MeshBatchPassMask& getPassMask() const { return m_pass_mask; }
        [[nodiscard]] const DynamicArray<MeshBatchElement>& getElements() const { return m_elements; }
        [[nodiscard]] Bool usesCustomBounds() const { return m_uses_custom_bounds; }
        [[nodiscard]] const Vector3f& getBoundsMin() const { return m_bounds_min; }
        [[nodiscard]] const Vector3f& getBoundsMax() const { return m_bounds_max; }

        [[nodiscard]] Bool isValid() const { return !m_elements.empty(); }
        [[nodiscard]] Bool isRelevant(const MeshPassType pass_type) const {
            return m_pass_mask.isRelevant(pass_type);
        }
    };

} // dodoe
