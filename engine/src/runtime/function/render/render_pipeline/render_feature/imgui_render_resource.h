// do@Redlive

#pragma once

#include "runtime/function/render/render_pipeline/render_pass_context.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/render/render_graph/render_graph_pass.h"

namespace dodoe {

    class PipelineStateCache;

    class ImGuiRenderResource {
        GfxTextureHandle m_font_texture{};
        GfxBufferHandle m_constant_buffer{};
        GfxBindingLayoutHandle m_binding_layout{};
        GfxGraphicsPipelineHandle m_pipeline{};
        GfxFramebufferHandle m_framebuffer{};
        GfxTextureHandle m_framebuffer_texture{};
        UnorderedMap<GfxTexture*, GfxBindingSetHandle> m_binding_sets{};

    public:
        void reset();

        [[nodiscard]] GfxTextureHandle getOrCreateFontTexture(
            DrawCommandList& command_list);

        [[nodiscard]] GfxBufferHandle getOrCreateConstantBuffer(
            DrawCommandList& command_list);

        [[nodiscard]] GfxBindingLayoutHandle getOrCreateBindingLayout(
            DrawCommandList& command_list);

        [[nodiscard]] GfxFramebufferHandle getOrCreateFramebuffer(
            DrawCommandList& command_list,
            const GfxTextureHandle& output);

        [[nodiscard]] GfxGraphicsPipelineHandle getOrCreatePipeline(
            PipelineStateCache* pipeline_cache,
            GfxShaderHandle imgui_vs,
            GfxShaderHandle imgui_ps,
            GfxInputLayoutHandle input_layout,
            const GfxFramebufferInfo& framebuffer_info,
            DrawCommandList& command_list);

        [[nodiscard]] GfxBindingSetHandle getOrCreateBindingSet(
            DrawCommandList& command_list,
            GfxTexture* texture);

        void invalidateBindingSets();
    };

} // dodoe
