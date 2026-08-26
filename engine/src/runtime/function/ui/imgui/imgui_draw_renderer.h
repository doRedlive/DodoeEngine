// do@Redlive

#ifdef DODOE_DEBUG_ENABLED

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    struct ImGuiRenderPacket;

    class DrawCommandList;
    class PipelineStateCache;
    class ShaderLibrary;

    class ImGuiDrawRenderer {
        GfxBindingLayoutHandle m_binding_layout{};
        GfxBindingSetHandle m_font_binding_set{};
        GfxInputLayoutHandle m_input_layout{};
        GfxTextureHandle m_font_texture{};
        GfxBufferHandle m_font_binding_set_cb{};

    public:
        ImGuiDrawRenderer() = default;
        ImGuiDrawRenderer(GfxBindingLayoutHandle binding_layout,
                          GfxBindingSetHandle font_binding_set,
                          GfxInputLayoutHandle input_layout,
                          GfxTextureHandle font_texture,
                          GfxBufferHandle font_binding_set_cb);

        void render(const ImGuiRenderPacket& packet,
                    const GfxFramebufferHandle& framebuffer,
                    const GfxFramebufferInfo& render_target_signature,
                    const GfxBufferHandle& vertex_buffer,
                    const GfxBufferHandle& index_buffer,
                    const GfxBufferHandle& constant_buffer,
                    DrawCommandList& command_list,
                    const PipelineStateCache* pipeline_cache,
                    const ShaderLibrary* shader_library);
    };

} // namespace dodoe

#endif // DODOE_DEBUG_ENABLED
