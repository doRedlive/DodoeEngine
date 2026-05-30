// do@Redlive

#pragma once

#include "dopch.h"

#include "../interface/rhi.h"
#include "../framework/descriptor_table_manager.h"

namespace dodoe {

    struct MeshPipelineStateDesc {
        String vertex_shader_path;
        String pixel_shader_path;
        String geometry_shader_path;

        Size_t constant_buffer_size{256};
        UInt32 constant_buffer_max_versions{4096};

        DynamicArray<rhi::BindingLayoutItem> extra_binding_items;
        DynamicArray<rhi::BindingSetItem> extra_binding_set_items;
        DescriptorTableManager* descriptor_table{nullptr};

        DynamicArray<rhi::VertexAttributeDesc> extra_vertex_attributes;
        DynamicArray<rhi::VertexAttributeDesc> vertex_attributes;

        rhi::RenderState render_state;
        rhi::PrimitiveType primitive_type{rhi::PrimitiveType::TriangleList};
        Bool disable_caching{false};
        String debug_name;
    };

    class MeshPipelineState {
        friend class MeshPassProcessor;

        MeshPipelineStateDesc m_desc;

        rhi::ShaderHandle m_vertex_shader;
        rhi::ShaderHandle m_pixel_shader;
        rhi::ShaderHandle m_geometry_shader;
        rhi::InputLayoutHandle m_input_layout;
        rhi::BindingLayoutHandle m_binding_layout;
        rhi::BindingSetHandle m_binding_set;
        rhi::BufferHandle m_constant_buffer;
        rhi::GraphicsPipelineHandle m_pipeline;
    public:
        [[nodiscard]] const rhi::GraphicsPipelineHandle& getPipeline() const { return m_pipeline; }
        [[nodiscard]] const rhi::BindingSetHandle& getBindingSet() const { return m_binding_set; }
        [[nodiscard]] const rhi::InputLayoutHandle& getInputLayout() const { return m_input_layout; }
        [[nodiscard]] const rhi::BindingLayoutHandle& getBindingLayout() const { return m_binding_layout; }
        [[nodiscard]] rhi::BufferHandle getConstantBuffer() const { return m_constant_buffer; }
        [[nodiscard]] const MeshPipelineStateDesc& getDesc() const { return m_desc; }

    };

} // dodoe
