// do@Redlive

#pragma once

#include "dopch.h"

#include "../baseline_pass.h"

namespace dodoe {

    class GfxContext;
    class BaselineImGuiPass;

    class BaselinePresentPass final : public BaselineRenderPass {
        GfxDeviceHandle m_device{};
        cutie::CommandListHandle m_command_list{};
        const ShaderLibrary* m_shader_library{nullptr};
        SharedRenderService* m_shared_render_service{nullptr};
        cutie::SamplerHandle m_sampler{};

        cutie::GraphicsPipelineHandle m_pipeline{};
        cutie::BindingLayoutHandle m_binding_layout{};
#ifdef DODOE_DEBUG_ENABLED
        BaselineImGuiPass* m_imgui_pass{nullptr};
#endif

    public:
        Bool initialize(const BaselinePassContext& context) override;
        void shutdown() override;

        void ensurePipeline(const cutie::FramebufferInfo& framebuffer_info);
        void setImGuiPass(BaselineImGuiPass* imgui_pass);
        void render(GfxContext& gfx, UInt32 swapchain_image_index,
                    const Vector2i& extent, const GfxTextureHandle& scene_color);
    };

} // namespace dodoe
