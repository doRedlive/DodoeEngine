// do@Redlive

#pragma once

#include "runtime/function/render/render_pipeline/render_pass_context.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/render/render_graph/render_graph_pass.h"

namespace dodoe {

    // Feature-owned persistent render resources for the editor overlay / ImGui path.
    // The RDG pass should stay thin and delegate all GPU resource lifecycle here.
    class ImGuiRenderResources {
        GfxBufferHandle m_vertex_buffer{};
        GfxBufferHandle m_index_buffer{};
        GfxTextureHandle m_font_texture{};
        GfxInputLayoutHandle m_input_layout{};
        GfxBindingLayoutHandle m_binding_layout{};
        GfxGraphicsPipelineHandle m_pipeline{};
        GfxFramebufferHandle m_framebuffer{};
        GfxTextureHandle m_framebuffer_texture{};
        UnorderedMap<GfxTexture*, GfxBindingSetHandle> m_binding_sets{};

    public:
        void reset();
        void renderImGui(
            const RenderPassContext& pass_context,
            const RenderGraphPassContext& context,
            DrawCommandList& command_list,
            const GfxTextureHandle& output);
    };

} // dodoe
