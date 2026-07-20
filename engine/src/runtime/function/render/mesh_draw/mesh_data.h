// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/render/material/material.h"

namespace dodoe {

    enum class MeshGeometryPrimitiveType : UInt8 {
        Triangles,
        Lines,
        LineStrip,
        Count
    };

    struct SubMesh {
        Ref<Material> material{};
        UInt32 index_offset{0};
        UInt32 vertex_offset{0};
        UInt32 index_count{0};
        UInt32 vertex_count{0};
        Int32 section_index{0};
        MeshGeometryPrimitiveType primitive_type{MeshGeometryPrimitiveType::Triangles};

        [[nodiscard]] Bool isValid() const { return index_count > 0; }
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

    struct Mesh {
        DynamicArray<MeshLODData> lods;

        [[nodiscard]] Bool isValid() const { return !lods.empty(); }

        [[nodiscard]] const MeshLODData* activeLOD(Float screen_size = 1.0f) const {
            for (Size_t i = 0; i < lods.size(); ++i) {
                if (lods[i].screen_size >= screen_size) {
                    return &lods[i];
                }
            }
            return lods.empty() ? nullptr : &lods.back();
        }
    };

} // namespace dodoe
