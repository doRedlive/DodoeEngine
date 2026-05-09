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
        DescriptorIndex index_buffer_descriptor;
        DescriptorIndex vertex_buffer_descriptor;
        DescriptorIndex instance_buffer_descriptor;
        std::vector<ui32> index_data;
        std::vector<Vector3f> position_data;
        std::vector<Vector2f> texcoord1_data;
        std::vector<Vector2f> texcoord2_data;
        std::vector<ui32> normal_data;
        std::vector<ui32> tangent_data;
    };

    enum class MeshGeometryPrimitiveType : ui8 {
        Triangles,
        Lines,
        LineStrip,

        Count
    };

    struct MeshGeometry {
        Ref<Material> material;
        ui32 index_offset{0};
        ui32 vertex_offset{0};
        ui32 index_count{0};
        ui32 vertex_count{0};
        int geometry_index{0};
        MeshGeometryPrimitiveType type{MeshGeometryPrimitiveType::Triangles};
    };

    enum class MeshType : ui8 {
        Triangles,
        CurvePolytubes,
        CurveDisjointOrthogonalTriangleStrips,
        CurveLinearSweptSpheres,

        Count
    };

    struct Mesh {
        std::string name;
        MeshType type{MeshType::Triangles};
        Ref<BufferGroup> buffers;
        std::vector<Ref<MeshGeometry>> geometries;
        ui32 index_offset{0};
        ui32 vertex_offset{0};
        ui32 index_count{0};
        ui32 vertex_count{0};
        int mesh_index{0};
    };

} // dodoe
