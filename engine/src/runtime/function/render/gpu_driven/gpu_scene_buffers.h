// do@Redlive

#pragma once

#include "dopch.h"
#include "gpu_object_id.h"
#include "runtime/core/math/math.h"
#include "runtime/function/render/render_scene/render_object.h"

namespace dodoe {

    struct alignas(16) GpuObjectMeta {
        UInt32 type;
        UInt32 flags;
        UInt32 material_id;
        UInt32 texture_id;
        UInt32 bounds_id;
        UInt32 data_offset;
        UInt32 reserved[2];
    };

    struct alignas(16) SpriteGpuData {
        Float position_x, position_y;
        Float scale_x, scale_y;
        Float rotation, _pad0;
        UInt32 atlas_index, _pad1;
        Float uv_min_x, uv_min_y;
        Float uv_max_x, uv_max_y;
        UInt32 color, sorting_key;
        UInt32 material_id, flags;
    };

    struct alignas(16) PrimitiveGpuData {
        UInt32 transform_index;
        UInt32 mesh_id;
        UInt32 section_start, section_count;
        UInt32 material_start, material_count;
    };

    struct alignas(16) GpuTransform {
        Matrix4f local_to_world;
        Matrix4f world_to_local;
    };

    struct alignas(16) GpuBounds {
        Vector3f center;
        Float sphere_radius;
        Vector3f extent;
        Float _pad;
    };

    struct alignas(16) LightGpuData {
        Vector3f position;
        Float radius;
        Vector3f direction;
        Float range;
        Vector3f color;
        Float intensity;
        Float inner_angle;
        Float outer_angle;
        UInt32 light_type;
        UInt32 cast_shadow;
        UInt32 cubemap_index;
        UInt32 _pad1;
    };

    struct alignas(16) CullingParams {
        Vector4f frustum_planes[6];
        UInt32 object_count;
        UInt32 padding[3];
    };

    struct alignas(16) DrawIndexedIndirectArgs {
        UInt32 index_count;
        UInt32 instance_count;
        UInt32 start_index_location;
        Int32 base_vertex_location;
        UInt32 start_instance_location;
    };

    struct alignas(16) BucketKey {
        UInt32 key;
        UInt32 _pad[3];
    };

    struct alignas(16) BucketCount {
        UInt32 instance_count;
        UInt32 current_offset;
        UInt32 _pad[2];
    };

} // dodoe
