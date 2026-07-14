// do@Redlive

#pragma once

#include "runtime/function/render/render_pipeline/render_pass_context.h"
#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

    class PipelineStateCache;

    class SpriteRenderResource {
        GfxBindingLayoutHandle m_binding_layout{};
        GfxGraphicsPipelineHandle m_pipeline{};
        GfxGraphicsPipelineHandle m_pipeline_traditional{};
        GfxFramebufferHandle m_framebuffer{};
        GfxTextureHandle m_framebuffer_texture{};

        GfxBindingLayoutHandle m_traditional_tex_layout{};

    public:
        void reset();

        GfxFramebufferHandle getOrCreateFramebuffer(
            DrawCommandList& command_list,
            const GfxTextureHandle& color_target);

        GfxBindingLayoutHandle getOrCreateBindingLayout(
            DrawCommandList& command_list);

        GfxGraphicsPipelineHandle getOrCreatePipeline(
            PipelineStateCache* pipeline_cache,
            GfxShaderHandle sprite_vs,
            GfxShaderHandle sprite_ps,
            GfxInputLayoutHandle input_layout,
            const GfxFramebufferInfo& framebuffer_info,
            DrawCommandList& command_list,
            GfxBindingLayoutHandle desc_table_layout);

        GfxBindingLayoutHandle getOrCreateTraditionalTextureLayout(
            DrawCommandList& command_list);

        GfxGraphicsPipelineHandle getOrCreateTraditionalPipeline(
            PipelineStateCache* pipeline_cache,
            GfxShaderHandle sprite_vs,
            GfxShaderHandle sprite_ps,
            GfxInputLayoutHandle input_layout,
            const GfxFramebufferInfo& framebuffer_info,
            DrawCommandList& command_list);
    };

} // dodoe
