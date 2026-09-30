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

    struct GizmoGridData {
        Bool ortho2d{false};
        Float minor_spacing{1.0f};
        Float major_spacing{10.0f};
        Float fade_begin{40.0f};
        Float fade_end{300.0f};
    };

    struct GizmoChannelData {
        DynamicArray<GizmoVertex> vertices;
        DynamicArray<UInt32> indices;
        DynamicArray<GizmoDrawCommand> commands;
        GizmoGridData grid{};
        Bool has_data{false};

        void clear() {
            vertices.clear();
            indices.clear();
            commands.clear();
            grid = GizmoGridData{};
            has_data = false;
        }
    };

    using GizmoChannel = DataChannel<GizmoChannelData>;

    DODOE_API GizmoChannel& GetGizmoChannel();

} // namespace dodoe

#endif // DODOE_EDITOR_ENABLED
