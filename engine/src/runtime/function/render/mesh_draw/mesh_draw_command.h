// do@Redlive

#pragma once

#include "dopch.h"

#include "mesh_batch.h"

namespace dodoe {

    struct MeshDrawCommand {
        rhi::GraphicsPipelineHandle pipeline;
        DynamicArray<rhi::BindingSetHandle> binding_sets;

        DynamicArray<rhi::VertexBufferBinding> vertex_bindings;
        rhi::IndexBufferBinding index_binding;
        rhi::DrawArguments draw_args;

        UInt64 sort_key{0};

        [[nodiscard]] Bool isValid() const {
            return pipeline && draw_args.vertexCount > 0;
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
