// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/render/framework/material.h"

namespace dodoe {

    enum class MeshGeometryPrimitiveType : UInt8 {
        Triangles,
        Lines,
        LineStrip,
        Count
    };

    struct MeshSection {
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
        DynamicArray<MeshSection> sections{};
        Float screen_size{1.0f};

        [[nodiscard]] Bool isValid() const {
            return buffers.isValid() && !sections.empty();
        }

        [[nodiscard]] UInt32 totalIndexCount() const {
            UInt32 count = 0;
            for (const auto& section : sections) {
                count += section.index_count;
            }
            return count;
        }
    };

} // dodoe
