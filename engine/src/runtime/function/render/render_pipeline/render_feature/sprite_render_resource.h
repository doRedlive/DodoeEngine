// do@Redlive

#pragma once

#include "runtime/function/render/render_pipeline/render_pass_context.h"
#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

    class PipelineStateCache;

    class SpriteRenderResource {
        GfxBindingLayoutHandle m_binding_layout{};
        GfxGraphicsPipelineHandle m_pipeline{};
        GfxFramebufferHandle m_framebuffer{};
        GfxTextureHandle m_framebuffer_texture{};

    public:
        void reset();

        [[nodiscard]] GfxFramebufferHandle getOrCreateFramebuffer(
            DrawCommandList& command_list,
            const GfxTextureHandle& color_target);

        [[nodiscard]] GfxBindingLayoutHandle getOrCreateBindingLayout(
            DrawCommandList& command_list);

        [[nodiscard]] GfxGraphicsPipelineHandle getOrCreatePipeline(
            PipelineStateCache* pipeline_cache,
            GfxShaderHandle sprite_vs,
            GfxShaderHandle sprite_ps,
            GfxInputLayoutHandle input_layout,
            const GfxFramebufferInfo& framebuffer_info,
            DrawCommandList& command_list,
            GfxBindingLayoutHandle desc_table_layout);
    };

} // dodoe
