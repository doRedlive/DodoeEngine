// do@Redlive

#pragma once

#include "dopch.h"

#include "../baseline_pass.h"

namespace dodoe {

    class RenderView;
    class RenderScene;

    class BaselineSkyPass final : public BaselineRenderPass {
        GfxDeviceHandle m_device{};
        cutie::CommandListHandle m_command_list{};
        const ShaderLibrary* m_shader_library{nullptr};
        SharedRenderService* m_shared_render_service{nullptr};
        cutie::SamplerHandle m_sampler{};

        cutie::GraphicsPipelineHandle m_pipeline{};
        cutie::BindingLayoutHandle m_binding_layout{};
        cutie::BufferHandle m_sky_cb{};

    public:
        Bool initialize(const BaselinePassContext& context) override;
        void shutdown() override;

        void ensurePipeline(const cutie::FramebufferInfo& framebuffer_info);
        void render(RenderView& view, RenderScene& scene, const GfxViewportState& viewport_state,
                    cutie::IFramebuffer* framebuffer, const GfxTextureHandle& gbuffer_depth);
    };

} // namespace dodoe
