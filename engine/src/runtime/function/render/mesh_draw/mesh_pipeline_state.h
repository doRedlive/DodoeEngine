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

        DynamicArray<GfxBindingLayoutItem> extra_binding_items;
        DynamicArray<GfxBindingSetItem> extra_binding_set_items;
        DescriptorTableManager* descriptor_table{nullptr};

        DynamicArray<GfxVertexAttributeDesc> extra_vertex_attributes;
        DynamicArray<GfxVertexAttributeDesc> vertex_attributes;

        GfxRenderState render_state;
        GfxPrimitiveType primitive_type{GfxPrimitiveType::TriangleList};
        Bool disable_caching{false};
        String debug_name;
    };

    class MeshPipelineState {
        friend class MeshPassProcessor;

        MeshPipelineStateDesc m_desc;

        GfxShaderHandle m_vertex_shader;
        GfxShaderHandle m_pixel_shader;
        GfxShaderHandle m_geometry_shader;
        GfxInputLayoutHandle m_input_layout;
        GfxBindingLayoutHandle m_binding_layout;
        GfxBindingSetHandle m_binding_set;
        GfxBufferHandle m_constant_buffer;
        GfxGraphicsPipelineHandle m_pipeline;
    public:
        [[nodiscard]] const GfxGraphicsPipelineHandle& getPipeline() const { return m_pipeline; }
        [[nodiscard]] const GfxBindingSetHandle& getBindingSet() const { return m_binding_set; }
        [[nodiscard]] const GfxInputLayoutHandle& getInputLayout() const { return m_input_layout; }
        [[nodiscard]] const GfxBindingLayoutHandle& getBindingLayout() const { return m_binding_layout; }
        [[nodiscard]] GfxBufferHandle getConstantBuffer() const { return m_constant_buffer; }
        [[nodiscard]] const MeshPipelineStateDesc& getDesc() const { return m_desc; }

    };

} // dodoe
