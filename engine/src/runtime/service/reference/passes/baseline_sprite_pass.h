// do@Redlive

#pragma once

#include "dopch.h"

#include "../baseline_pass.h"

namespace dodoe {

    class RenderView;
    class RenderScene;
    struct SpriteInstance;

    class BaselineSpritePass final : public BaselineRenderPass {
        GfxDeviceHandle m_device{};
        cutie::CommandListHandle m_command_list{};
        const ShaderLibrary* m_shader_library{nullptr};
        SharedRenderService* m_shared_render_service{nullptr};
        cutie::SamplerHandle m_sampler{};

        cutie::GraphicsPipelineHandle m_pipeline{};
        cutie::BindingLayoutHandle m_cb_binding_layout{};
        cutie::BindingLayoutHandle m_material_binding_layout{};
        cutie::InputLayoutHandle m_input_layout{};
        cutie::BufferHandle m_instance_buffer{};
        cutie::BufferHandle m_vp_buffer{};
        UInt32 m_instance_capacity{0};

    public:
        Bool initialize(const BaselinePassContext& context) override;
        void shutdown() override;

        void ensurePipeline(const cutie::FramebufferInfo& framebuffer_info);
        void render(RenderView& view, RenderScene& scene,
                    cutie::IFramebuffer* framebuffer, const GfxViewportState& viewport_state);

    private:
        void ensureInstanceCapacity(UInt32 instance_count);
        void collectInstances(RenderView& view, RenderScene& scene,
                              DynamicArray<SpriteInstance>& out_instances);
    };

} // namespace dodoe
