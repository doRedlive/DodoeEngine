// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/service/reference/baseline_pass.h"

namespace dodoe {

    class RenderView;
    class ShaderLibrary;
    class SharedRenderService;

    class BaselineTaaPass {
    public:
        Bool initialize(const BaselinePassContext& context);
        void shutdown();
        void ensurePipeline(const cutie::FramebufferInfo& framebuffer_info);
        void render(RenderView& view, const Vector2i& extent,
                    const GfxTextureHandle& scene_color,
                    const GfxTextureHandle& gbuffer_position,
                    const GfxTextureHandle& gbuffer_depth,
                    const GfxTextureHandle& gbuffer_motion,
                    const GfxTextureHandle& history_read,
                    const GfxTextureHandle& history_write,
                    cutie::IFramebuffer* history_write_framebuffer,
                    const GfxViewportState& viewport_state,
                    Bool targets_recreated);

    private:
        GfxDeviceHandle m_device{};
        cutie::CommandListHandle m_command_list{};
        const ShaderLibrary* m_shader_library{nullptr};
        SharedRenderService* m_shared_render_service{nullptr};
        cutie::SamplerHandle m_sampler{};
        cutie::BindingLayoutHandle m_binding_layout{};
        cutie::BindingLayoutHandle m_copy_binding_layout{};
        cutie::GraphicsPipelineHandle m_pipeline{};
        cutie::GraphicsPipelineHandle m_copy_pipeline{};
        cutie::BufferHandle m_taa_cb{};
        GfxTextureHandle m_prev_depth_a{};
        GfxTextureHandle m_prev_depth_b{};
        GfxFramebufferHandle m_prev_depth_framebuffer_a{};
        GfxFramebufferHandle m_prev_depth_framebuffer_b{};
        Vector2i m_prev_depth_extent{0, 0};
        UInt64 m_frame_counter{0};
        Matrix4f m_prev_unjittered_view_projection{1.0f};
        Vector2f m_prev_jitter_uv{0.0f, 0.0f};
        Bool m_has_prev_frame{false};

        void ensurePrevDepthTargets(const Vector2i& extent);
        void renderPrevDepthCopy(const GfxTextureHandle& depth,
                                 cutie::IFramebuffer* prev_depth_write_framebuffer,
                                 const GfxViewportState& viewport_state);
    };

} // namespace dodoe
