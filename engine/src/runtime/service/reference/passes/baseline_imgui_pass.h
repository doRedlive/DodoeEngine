// do@Redlive

#pragma once

#include "dopch.h"

#ifdef DODOE_DEBUG_ENABLED

#include "../baseline_pass.h"

namespace dodoe {

    class ShaderLibrary;

    class BaselineImGuiPass final : public BaselineRenderPass {
        GfxDeviceHandle m_device{};
        cutie::CommandListHandle m_command_list{};
        const ShaderLibrary* m_shader_library{nullptr};
        cutie::SamplerHandle m_sampler{};

        cutie::GraphicsPipelineHandle m_pipeline{};
        cutie::BindingLayoutHandle m_binding_layout{};
        cutie::InputLayoutHandle m_input_layout{};
        cutie::BufferHandle m_vertex_buffer{};
        cutie::BufferHandle m_index_buffer{};
        cutie::BufferHandle m_constant_buffer{};

    public:
        Bool initialize(const BaselinePassContext& context) override;
        void shutdown() override;

        void ensurePipeline(const cutie::FramebufferInfo& framebuffer_info);
        void render(cutie::IFramebuffer* framebuffer);
    };

} // namespace dodoe

#endif // DODOE_DEBUG_ENABLED
