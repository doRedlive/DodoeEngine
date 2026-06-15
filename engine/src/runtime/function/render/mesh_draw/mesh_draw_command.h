// do@Redlive

#pragma once

#include "dopch.h"
#include "runtime/function/graphics/gfx.h"

#include "mesh_pass_type.h"

namespace dodoe {

    struct MeshDrawCommand {
        MeshPassType pass_type{MeshPassType::GBuffer};
        GfxGraphicsPipelineHandle pipeline;
        DynamicArray<GfxBindingSetHandle> binding_sets;

        DynamicArray<GfxVertexBufferBinding> vertex_bindings;
        UInt32 primitive_scene_buffer_slot{1};
        UInt64 primitive_scene_buffer_offset{0};
        Bool uses_primitive_scene_buffer{false};
        GfxIndexBufferBinding index_binding;
        GfxDrawArguments draw_args;
        UInt32 primitive_index{0};
        UInt32 shader_data_index{std::numeric_limits<UInt32>::max()};

        UInt64 sort_key{0};

        void setPrimitiveSceneBufferBinding(const UInt32 slot, const UInt64 offset) {
            primitive_scene_buffer_slot = slot;
            primitive_scene_buffer_offset = offset;
            uses_primitive_scene_buffer = true;
        }

        [[nodiscard]] Bool isValid() const {
            return draw_args.vertexCount > 0;
        }

        [[nodiscard]] Bool hasShaderData() const {
            return shader_data_index != std::numeric_limits<UInt32>::max();
        }

        [[nodiscard]] Bool usesPassPipeline() const {
            return !pipeline;
        }
    };

    struct MeshDrawCommandCacheKey {
        Size_t batch_hash{0};
        Size_t material_hash{0};
        Size_t pass_hash{0};

        Bool operator==(const MeshDrawCommandCacheKey& other) const {
            return batch_hash == other.batch_hash &&
                   material_hash == other.material_hash &&
                   pass_hash == other.pass_hash;
        }
    };

} // dodoe

template <>
struct std::hash<dodoe::MeshDrawCommandCacheKey> {
    dodoe::Size_t operator()(const dodoe::MeshDrawCommandCacheKey& k) const {
        return k.batch_hash ^ (k.material_hash << 1) ^ (k.pass_hash << 3);
    }
};
