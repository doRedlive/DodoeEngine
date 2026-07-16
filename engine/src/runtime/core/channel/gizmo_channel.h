// do@Redlive

#pragma once

#include "dopch.h"

#ifdef DODOE_EDITOR_ENABLED

#include "base_channel.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    struct GizmoVertex {
        Float px, py, pz;
        Float r, g, b, a;
    };

    struct GizmoDrawCommand {
        UInt32 vertex_offset;
        UInt32 vertex_count;
        UInt32 index_offset;
        UInt32 index_count;
        Matrix4f transform{1.0f};
        GfxPrimitiveType topology{GfxPrimitiveType::LineList};
    };

    struct GizmoChannelData {
        DynamicArray<GizmoVertex> vertices;
        DynamicArray<UInt32> indices;
        DynamicArray<GizmoDrawCommand> commands;
        Bool has_data{false};

        void clear() {
            vertices.clear();
            indices.clear();
            commands.clear();
            has_data = false;
        }
    };

    using GizmoChannel = DataChannel<GizmoChannelData>;

    inline GizmoChannel& GetGizmoChannel() {
        static GizmoChannel channel;
        return channel;
    }

} // namespace dodoe

#endif // DODOE_EDITOR_ENABLED
