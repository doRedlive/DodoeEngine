// do@Redlive

#pragma once

#include "dopch.h"

#include "../baseline_pass.h"

namespace dodoe {

    class RenderView;
    class RenderViewFamily;
    class RenderScene;
    struct MaterialInstance;

    // UE BasePass equivalent: rasterizes opaque geometry into the GBuffer.
    class BaselineGBufferPass final : public BaselineRenderPass {
        GfxDeviceHandle m_device{};
        cutie::CommandListHandle m_command_list{};
        const ShaderLibrary* m_shader_library{nullptr};
        SharedRenderService* m_shared_render_service{nullptr};

        cutie::GraphicsPipelineHandle m_pipeline{};
        cutie::InputLayoutHandle m_input_layout{};
        cutie::BindingLayoutHandle m_global_binding_layout{};
        cutie::BindingLayoutHandle m_view_binding_layout{};
        cutie::BindingLayoutHandle m_material_binding_layout{};
        cutie::BindingLayoutHandle m_primitive_binding_layout{};
        cutie::BindingLayoutHandle m_bindless_binding_layout{};
        cutie::BufferHandle m_global_cb{};
        cutie::BufferHandle m_view_cb{};
        cutie::BufferHandle m_primitive_cb{};
        cutie::BufferHandle m_instance_buffer{};
        UInt32 m_instance_capacity{0};
        cutie::BindingSetHandle m_global_binding_set{};
        cutie::BindingSetHandle m_view_binding_set{};
        cutie::BindingSetHandle m_primitive_binding_set{};
        GfxBindingSetHandle m_material_binding_set{};
        GfxBindingSetHandle m_bindless_binding_set{};
        Bool m_material_warning_logged{false};

    public:
        Bool initialize(const BaselinePassContext& context) override;
        void shutdown() override;

        void ensurePipeline(const cutie::FramebufferInfo& framebuffer_info);
        void setupView(RenderView& view, RenderViewFamily& view_family);
        void render(RenderView& view, RenderScene& scene,
                    const GfxViewportState& viewport_state, cutie::IFramebuffer* framebuffer);

    private:
        void ensureInstanceCapacity(UInt32 instance_count);
    };

} // namespace dodoe
