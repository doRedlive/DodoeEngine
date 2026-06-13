// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"
#include "../framework/descriptor_table_manager.h"

namespace dodoe {

    struct MeshPipelineStateDesc {
        String vertex_shader_path;
        String pixel_shader_path;
        String geometry_shader_path;

        Size_t constant_buffer_size{256};
        UInt32 constant_buffer_max_versions{4096};

        DynamicArray<gfx::BindingLayoutItem> extra_binding_items;
        DynamicArray<gfx::BindingSetItem> extra_binding_set_items;
        DescriptorTableManager* descriptor_table{nullptr};

        DynamicArray<gfx::VertexAttributeDesc> extra_vertex_attributes;
        DynamicArray<gfx::VertexAttributeDesc> vertex_attributes;

        gfx::RenderState render_state;
        gfx::PrimitiveType primitive_type{gfx::PrimitiveType::TriangleList};
        Bool disable_caching{false};
        String debug_name;
    };

    class MeshPipelineState {
        friend class MeshPassProcessor;

        MeshPipelineStateDesc m_desc;

        gfx::ShaderHandle m_vertex_shader;
        gfx::ShaderHandle m_pixel_shader;
        gfx::ShaderHandle m_geometry_shader;
        gfx::InputLayoutHandle m_input_layout;
        gfx::BindingLayoutHandle m_binding_layout;
        gfx::BindingSetHandle m_binding_set;
        gfx::BufferHandle m_constant_buffer;
        gfx::GraphicsPipelineHandle m_pipeline;
    public:
        [[nodiscard]] const gfx::GraphicsPipelineHandle& getPipeline() const { return m_pipeline; }
        [[nodiscard]] const gfx::BindingSetHandle& getBindingSet() const { return m_binding_set; }
        [[nodiscard]] const gfx::InputLayoutHandle& getInputLayout() const { return m_input_layout; }
        [[nodiscard]] const gfx::BindingLayoutHandle& getBindingLayout() const { return m_binding_layout; }
        [[nodiscard]] gfx::BufferHandle getConstantBuffer() const { return m_constant_buffer; }
        [[nodiscard]] const MeshPipelineStateDesc& getDesc() const { return m_desc; }

    };

} // dodoe
