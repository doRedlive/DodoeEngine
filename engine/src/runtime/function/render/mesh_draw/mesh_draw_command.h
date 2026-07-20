// do@Redlive

#pragma once

#include "dopch.h"
#include "mesh_pass_type.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

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

    struct MeshDrawCommand {
        MeshPassType pass_type{MeshPassType::GBuffer};
        GfxGraphicsPipelineHandle pipeline;
        DynamicArray<GfxBindingSetHandle> binding_sets;
        DynamicArray<GfxVertexBufferBinding> vertex_bindings;
        GfxIndexBufferBinding index_binding;
        GfxDrawArguments draw_args;
    };

    struct MeshDrawInstance {
        UInt32 cmd_index{0};
        UInt32 shader_data_index{std::numeric_limits<UInt32>::max()};
        UInt64 instance_offset{0};

        [[nodiscard]] Bool hasShaderData() const {
            return shader_data_index != std::numeric_limits<UInt32>::max();
        }
    };

} // dodoe

template <>
struct std::hash<dodoe::MeshDrawCommandCacheKey> {
    dodoe::Size_t operator()(const dodoe::MeshDrawCommandCacheKey& k) const {
        return k.batch_hash ^ (k.material_hash << 1) ^ (k.pass_hash << 3);
    }
};
