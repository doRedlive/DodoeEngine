// do@Redlive

#include "local_vertex_factory.h"

#include "runtime/function/render/mesh_draw/view_mesh_draw_context.h"

namespace dodoe {

    void LocalVertexFactory::initialize(
        GfxContext& gfx_context,
        const GfxShaderHandle& gbuffer_vertex_shader,
        const GfxShaderHandle& shadow_vertex_shader)
    {
        const auto device = gfx_context.getDevice();
        DO_ASSERT(device != nullptr, "LocalVertexFactory device is null");

        constexpr Size_t kVertexStride = sizeof(Vector3f) + sizeof(UInt32) + sizeof(Vector2f);
        constexpr Size_t kInstanceStride = sizeof(InstanceSceneData);
        DynamicArray<VertexAttributeDesc> vertex_attributes = {
            VertexAttributeDesc().setName("a_Position").setFormat(Format::RGB32_FLOAT).setOffset(0).setElementStride(kVertexStride),
            VertexAttributeDesc().setName("a_Normal").setFormat(Format::RGBA8_SNORM).setOffset(sizeof(Vector3f)).setElementStride(kVertexStride),
            VertexAttributeDesc().setName("a_UV").setFormat(Format::RG32_FLOAT).setOffset(sizeof(Vector3f) + sizeof(UInt32)).setElementStride(kVertexStride),
            VertexAttributeDesc().setName("a_Model0").setFormat(Format::RGBA32_FLOAT).setBufferIndex(1).setOffset(0).setElementStride(kInstanceStride).setIsInstanced(true),
            VertexAttributeDesc().setName("a_Model1").setFormat(Format::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Vector4f)).setElementStride(kInstanceStride).setIsInstanced(true),
            VertexAttributeDesc().setName("a_Model2").setFormat(Format::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Vector4f) * 2).setElementStride(kInstanceStride).setIsInstanced(true),
            VertexAttributeDesc().setName("a_Model3").setFormat(Format::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Vector4f) * 3).setElementStride(kInstanceStride).setIsInstanced(true),
            VertexAttributeDesc().setName("a_InstanceColorTint").setFormat(Format::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Matrix4f)).setElementStride(kInstanceStride).setIsInstanced(true),
            VertexAttributeDesc().setName("a_InstanceParams").setFormat(Format::RGBA32_FLOAT).setBufferIndex(1).setOffset(sizeof(Matrix4f) + sizeof(Vector4f)).setElementStride(kInstanceStride).setIsInstanced(true),
        };

        m_gbuffer_input_layout = device->createInputLayout(
            vertex_attributes.data(),
            static_cast<UInt32>(vertex_attributes.size()),
            gbuffer_vertex_shader
        );
        m_shadow_input_layout = device->createInputLayout(
            vertex_attributes.data(),
            static_cast<UInt32>(vertex_attributes.size()),
            shadow_vertex_shader
        );
    }

    void LocalVertexFactory::reset() {
        m_shadow_input_layout = nullptr;
        m_gbuffer_input_layout = nullptr;
    }

} // dodoe
