// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/core/object/pptr.h"
#include "runtime/function/render/material/material.h"

namespace dodoe {
    class Material;
}

namespace dodoe {

    enum class MeshGeometryPrimitiveType : UInt8 {
        Triangles,
        Lines,
        LineStrip,
        Count
    };

    struct SubMesh {
        PPtr<Material> material{};
        UInt32 index_offset{0};
        UInt32 vertex_offset{0};
        UInt32 index_count{0};
        UInt32 vertex_count{0};
        Int32 section_index{0};
        MeshGeometryPrimitiveType primitive_type{MeshGeometryPrimitiveType::Triangles};

        [[nodiscard]] Bool isValid() const { return index_count > 0; }
    };

    struct MeshUploadData {
        String name{};
        DynamicArray<Vector3f> position_data{};
        DynamicArray<Vector2f> texcoord_data{};
        DynamicArray<UInt32> normal_data{};
        DynamicArray<UInt32> index_data{};
    };

    struct MeshBufferData {
        GfxBufferHandle vertex_buffer{};
        GfxBufferHandle index_buffer{};

        [[nodiscard]] Bool isValid() const { return vertex_buffer && index_buffer; }
    };

    struct MeshLODData {
        MeshBufferData buffers{};
        DynamicArray<SubMesh> sub_meshes{};
        Float screen_size{1.0f};

        [[nodiscard]] Bool isValid() const {
            return buffers.isValid() && !sub_meshes.empty();
        }

        [[nodiscard]] UInt32 totalIndexCount() const {
            UInt32 count = 0;
            for (const auto& sm : sub_meshes) {
                count += sm.index_count;
            }
            return count;
        }
    };

} // namespace dodoe
