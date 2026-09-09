// do@Redlive

#pragma once

#include "dopch.h"

#include "../baseline_pass.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/render/render_pipeline/shadow/shadow_system.h"

namespace dodoe {

    class RenderView;
    class RenderScene;

    struct BaselineShadowResult {
        GfxTextureHandle shadow_map{};
        StaticArray<Matrix4f, kShadowCascadeCount> cascade_view_projections{
            Matrix4f(1.0f), Matrix4f(1.0f), Matrix4f(1.0f), Matrix4f(1.0f)};
        Vector4f cascade_split_depths{0.0f};
        Vector4f shadow_params{0.005f, 0.2f, 0.0f, 2.0f};
        Bool has_shadow{false};
    };

    class BaselineShadowPass final : public BaselineRenderPass {
        GfxDeviceHandle m_device{};
        cutie::CommandListHandle m_command_list{};
        const ShaderLibrary* m_shader_library{nullptr};
        SharedRenderService* m_shared_render_service{nullptr};

        cutie::GraphicsPipelineHandle m_pipeline{};
        cutie::InputLayoutHandle m_input_layout{};
        cutie::BindingLayoutHandle m_global_binding_layout{};
        cutie::BindingLayoutHandle m_view_binding_layout{};
        cutie::BufferHandle m_global_cb{};
        cutie::BufferHandle m_view_cb{};
        cutie::BindingSetHandle m_global_binding_set{};
        cutie::BindingSetHandle m_view_binding_set{};

        cutie::BufferHandle m_instance_buffer{};
        UInt32 m_instance_capacity{0};

        static constexpr UInt32 kShadowMapSize = 2048;
        GfxTextureHandle m_shadow_depth{};
        GfxFramebufferHandle m_shadow_framebuffer{};

    public:
        Bool initialize(const BaselinePassContext& context) override;
        void shutdown() override;

        void ensurePipeline();
        BaselineShadowResult render(RenderView& view, RenderScene& scene);

    private:
        Bool ensureShadowTarget();
        void ensureInstanceCapacity(UInt32 instance_count);
    };

} // namespace dodoe
