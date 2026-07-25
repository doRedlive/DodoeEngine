// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"
#include "mesh_pass_type.h"

namespace dodoe {

    class MaterialSystem;
    struct MaterialInstance;
    struct MaterialProperties;

namespace dodoe {

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
    };

    struct MeshBatch {
        Identifier primitive_id{};
        MaterialInstance* material_instance{nullptr};
        MeshBatchPassMask pass_mask{};
        DynamicArray<MeshBatchElement> elements;
        Bool uses_custom_bounds{false};
        Vector3f bounds_min{0.0f};
        Vector3f bounds_max{0.0f};

        [[nodiscard]] Bool isValid() const {
            return !elements.empty();
        }

        [[nodiscard]] Bool isRelevant(const MeshPassType pass_type) const {
            return pass_mask.isRelevant(pass_type);
        }
    };

} // dodoe
