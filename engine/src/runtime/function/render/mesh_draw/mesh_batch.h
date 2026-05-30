// do@Redlive

#pragma once

#include "dopch.h"

#include "../interface/rhi.h"
#include "runtime/resource/resource_type.h"

namespace dodoe {

    struct MeshBatchElement {
        static constexpr Size_t kMaxVertexBufferSlots = 16;

        UInt32 index_count{0};
        UInt32 index_offset{0};
        UInt32 vertex_offset{0};
        UInt32 first_instance{0};
        UInt32 num_instances{0};

        rhi::BufferHandle vertex_buffers[kMaxVertexBufferSlots];
        rhi::BufferHandle index_buffer;

        [[nodiscard]] Bool isValid() const {
            return index_count > 0 && num_instances > 0 &&
                   vertex_buffers[0] && index_buffer;
        }
    };

    struct MeshBatch {
        Ref<Material> material;
        DynamicArray<MeshBatchElement> elements;

        [[nodiscard]] Bool isValid() const {
            return !elements.empty();
        }
    };

} // dodoe
