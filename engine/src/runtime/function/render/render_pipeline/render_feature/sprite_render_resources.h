// do@Redlive

#pragma once

#include "runtime/function/render/render_pipeline/render_pass_context.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/render/render_graph/render_graph_pass.h"

namespace dodoe {

    class SpriteRenderResources {
        GfxInputLayoutHandle m_input_layout{};
        GfxBindingLayoutHandle m_binding_layout{};
        GfxBindingSetHandle m_binding_set{};
        GfxGraphicsPipelineHandle m_pipeline{};
        GfxFramebufferHandle m_framebuffer{};
        GfxTextureHandle m_framebuffer_texture{};
        GfxBufferHandle m_bound_vp_buffer{};

    public:
        void reset();
        void renderSprites(
            const RenderPassContext& pass_context,
            const RenderGraphPassContext& context,
            DrawCommandList& command_list,
            const GfxTextureHandle& color_target,
            const GfxBufferHandle& quad_vertex_buffer,
            const GfxBufferHandle& quad_index_buffer,
            const GfxBufferHandle& instance_buffer,
            const GfxBufferHandle& vp_buffer,
            UInt32 visible_count
        );
    };

} // dodoe
