// do@Redlive

#pragma once

#include "dopch.h"

#include "../baseline_pass.h"

namespace dodoe {

    class RenderView;

    class BaselineOutlinePass final : public BaselineRenderPass {
        GfxDeviceHandle m_device{};
        cutie::CommandListHandle m_command_list{};
        const ShaderLibrary* m_shader_library{nullptr};
        SharedRenderService* m_shared_render_service{nullptr};
        cutie::SamplerHandle m_sampler{};

        cutie::BindingLayoutHandle m_binding_layout{};
        cutie::GraphicsPipelineHandle m_pipeline{};

    public:
        Bool initialize(const BaselinePassContext& context) override;
        void shutdown() override;

        void ensurePipeline(const cutie::FramebufferInfo& framebuffer_info);
        void render(RenderView& view, const Vector2i& extent,
                    const GfxTextureHandle& selection_mask, cutie::IFramebuffer* framebuffer);
    };

} // namespace dodoe
