// do@Redlive

#pragma once

#include "dopch.h"

#include "../baseline_pass.h"

namespace dodoe {

    class RenderView;

    // UE PostProcessPass equivalent: tone mapping + FXAA anti-aliasing.
    // Reads the HDR scene color, writes the LDR scene color (ping-pong).
    class BaselinePostProcessPass final : public BaselineRenderPass {
        GfxDeviceHandle m_device{};
        cutie::CommandListHandle m_command_list{};
        const ShaderLibrary* m_shader_library{nullptr};
        SharedRenderService* m_shared_render_service{nullptr};
        cutie::SamplerHandle m_sampler{};

        cutie::BindingLayoutHandle m_binding_layout{};
        cutie::GraphicsPipelineHandle m_tone_map_pipeline{};
        cutie::GraphicsPipelineHandle m_fxaa_pipeline{};

    public:
        Bool initialize(const BaselinePassContext& context) override;
        void shutdown() override;

        void ensurePipelines(const cutie::FramebufferInfo& framebuffer_info);
        void render(RenderView& view, const Vector2i& extent,
                    const GfxTextureHandle& hdr_color,
                    const GfxTextureHandle& tone_map_color, cutie::IFramebuffer* tone_map_framebuffer,
                    const GfxTextureHandle& fxaa_color, cutie::IFramebuffer* fxaa_framebuffer);
    };

} // namespace dodoe
