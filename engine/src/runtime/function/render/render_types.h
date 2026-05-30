// do@Redlive
#pragma once

#include "dopch.h"

#include "runtime/resource/resource_type.h"
#include "framework/descriptor_table_manager.h"
#include "interface/rhi.h"

namespace dodoe {

    struct BufferGroup {
        rhi::BufferHandle index_buffer;
        rhi::BufferHandle vertex_buffer;
        rhi::BufferHandle instance_buffer;
        rhi::BufferHandle instance_id_buffer;
        DescriptorIndex index_buffer_descriptor;
        DescriptorIndex vertex_buffer_descriptor;
        DescriptorIndex instance_buffer_descriptor;
        DynamicArray<UInt32> index_data;
        DynamicArray<Vector3f> position_data;
        DynamicArray<Vector2f> texcoord1_data;
        DynamicArray<Vector2f> texcoord2_data;
        DynamicArray<UInt32> normal_data;
        DynamicArray<UInt32> tangent_data;
    };

    enum class MeshGeometryPrimitiveType : UInt8 {
        Triangles,
        Lines,
        LineStrip,
        Count
    };

    struct MeshGeometry {
        Ref<Material> material;
        UInt32 index_offset{0};
        UInt32 vertex_offset{0};
        UInt32 index_count{0};
        UInt32 vertex_count{0};
        Int32 geometry_index{0};
        MeshGeometryPrimitiveType type{MeshGeometryPrimitiveType::Triangles};
    };

    enum class MeshType : UInt8 {
        Triangles,
        CurvePolytubes,
        CurveDisjointOrthogonalTriangleStrips,
        CurveLinearSweptSpheres,
        Count
    };

    struct Mesh {
        String name;
        MeshType type{MeshType::Triangles};
        Ref<BufferGroup> buffers;
        DynamicArray<Ref<MeshGeometry>> geometries;
        UInt32 index_offset{0};
        UInt32 vertex_offset{0};
        UInt32 index_count{0};
        UInt32 vertex_count{0};
        Int32 mesh_index{0};
    };

} // dodoe
