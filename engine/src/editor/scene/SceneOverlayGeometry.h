// do@Redlive
#pragma once
#include "runtime/core/channel/gizmo_channel.h"
#include "runtime/core/utils/util.h"

namespace cakery::scene_overlay {
using namespace dodoe;

[[nodiscard]] inline GizmoVertex MakeVertex(const Vector3f& pos, const Color& color) {
    return {pos.x, pos.y, pos.z, color.r, color.g, color.b, color.a};
}

inline void AddLine(GizmoChannelData& data, const Vector3f& start, const Vector3f& end,
             const Color& color) {
    const UInt32 base = static_cast<UInt32>(data.vertices.size());
    data.vertices.push_back(MakeVertex(start, color));
    data.vertices.push_back(MakeVertex(end, color));

    GizmoDrawCommand cmd;
    cmd.vertex_offset = base;
    cmd.vertex_count  = 2;
    cmd.index_offset  = 0;
    cmd.index_count   = 0;
    cmd.topology      = GfxPrimitiveType::LineList;
    cmd.transform     = Matrix4f(1.0f);
    data.commands.push_back(cmd);
}


} // namespace cakery::scene_overlay
