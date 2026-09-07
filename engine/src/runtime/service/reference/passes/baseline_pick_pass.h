// do@Redlive

#pragma once

#include "dopch.h"

#include "../baseline_pass.h"

namespace dodoe {

    class RenderView;

    class BaselinePickPass final : public BaselineRenderPass {
        GfxDeviceHandle m_device{};
        cutie::CommandListHandle m_command_list{};
        const ShaderLibrary* m_shader_library{nullptr};
        SharedRenderService* m_shared_render_service{nullptr};

        cutie::GraphicsPipelineHandle m_pipeline{};
        cutie::InputLayoutHandle m_input_layout{};
        cutie::BindingLayoutHandle m_view_binding_layout{};
        cutie::BindingLayoutHandle m_primitive_binding_layout{};
        cutie::BufferHandle m_view_cb{};
        cutie::BufferHandle m_primitive_cb{};
        cutie::BindingSetHandle m_view_binding_set{};
        cutie::BindingSetHandle m_primitive_binding_set{};
        Bool m_warning_logged{false};

    public:
        Bool initialize(const BaselinePassContext& context) override;
        void shutdown() override;

        void ensurePipeline(const cutie::FramebufferInfo& framebuffer_info);
        Bool render(RenderView& view, const GfxViewportState& viewport_state,
                    cutie::IFramebuffer* framebuffer, cutie::IBuffer* instance_buffer,
                    DynamicArray<UInt64>& out_pick_ids);
    };

} // namespace dodoe
