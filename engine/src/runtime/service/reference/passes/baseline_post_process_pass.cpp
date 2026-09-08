// do@Redlive

#include "baseline_post_process_pass.h"

#include "runtime/function/render/render_frame/frame_telemetry.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/render_service/binding_layout_cache.h"
#include "runtime/function/render/render_view/render_view.h"
#include "runtime/function/render/render_pipeline/render_pipeline_pass_utils.h"

namespace dodoe {

    Bool BaselinePostProcessPass::initialize(const BaselinePassContext& context) {
        m_device = context.device;
        m_command_list = context.command_list;
        m_shader_library = context.shader_library;
        m_shared_render_service = context.shared_render_service;
        m_sampler = context.sampler;

        if (!m_shared_render_service || !m_shader_library) {
            return true;
        }
        auto* binding_layout_cache = m_shared_render_service->getBindingLayoutCache();
        DO_ASSERT(binding_layout_cache != nullptr, "BaselinePostProcessPass: binding layout cache is unavailable");
        m_binding_layout = binding_layout_cache->getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::Pixel)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Pass))
                .addItem(GfxBindingLayoutItem::Texture_SRV(1))
                .addItem(GfxBindingLayoutItem::Sampler(9)));
        return true;
    }

    void BaselinePostProcessPass::shutdown() {
        m_binding_layout = nullptr;
        m_tone_map_pipeline = nullptr;
        m_fxaa_pipeline = nullptr;
        m_sampler = nullptr;
        m_shader_library = nullptr;
        m_shared_render_service = nullptr;
        m_command_list = nullptr;
        m_device = nullptr;
    }

    void BaselinePostProcessPass::ensurePipelines(const cutie::FramebufferInfo& framebuffer_info) {
        if (!m_shader_library) {
            return;
        }
        const auto vertex_shader = m_shader_library->getFullscreenVertexShader();
        if (!vertex_shader) {
            DO_ERROR("BaselinePostProcessPass: fullscreen vertex shader is not loaded");
            return;
        }

        if (!m_tone_map_pipeline) {
            const auto pixel_shader = m_shader_library->getToneMappingPixelShader();
            if (!pixel_shader) {
                DO_ERROR("BaselinePostProcessPass: tone mapping shader is not loaded");
            } else {
                const auto pipeline_desc = rendering_pipeline_utils::BuildFullscreenPipelineDesc(
                    vertex_shader, pixel_shader, m_binding_layout);
                m_tone_map_pipeline = m_device->createGraphicsPipeline(pipeline_desc, framebuffer_info);
                DO_INFO("BaselinePostProcessPass: tone mapping pipeline created");
            }
        }

        if (!m_fxaa_pipeline) {
            const auto pixel_shader = m_shader_library->getFxaaPixelShader();
            if (!pixel_shader) {
                DO_ERROR("BaselinePostProcessPass: fxaa shader is not loaded");
            } else {
                const auto pipeline_desc = rendering_pipeline_utils::BuildFullscreenPipelineDesc(
                    vertex_shader, pixel_shader, m_binding_layout);
                m_fxaa_pipeline = m_device->createGraphicsPipeline(pipeline_desc, framebuffer_info);
                DO_INFO("BaselinePostProcessPass: fxaa pipeline created");
            }
        }
    }

    void BaselinePostProcessPass::render(RenderView& view, const Vector2i& extent,
                                         const GfxTextureHandle& hdr_color,
                                         const GfxTextureHandle& tone_map_color, cutie::IFramebuffer* tone_map_framebuffer,
                                         const GfxTextureHandle& fxaa_color, cutie::IFramebuffer* fxaa_framebuffer) {
        const auto viewport_state = rendering_pipeline_utils::BuildViewportState(view, extent);

        // Tone mapping: HDR -> LDR
        m_command_list->setTextureState(tone_map_color->getRHI(), cutie::AllSubresources, cutie::ResourceStates::RenderTarget);
        RenderFrameCounters::Self().addBarrier(); m_command_list->commitBarriers();
        m_command_list->clearTextureFloat(tone_map_color->getRHI(), cutie::AllSubresources, cutie::Color(0.0f, 0.0f, 0.0f, 1.0f));

        if (m_tone_map_pipeline) {
            auto binding_set = m_device->createBindingSet(
                GfxBindingSetDesc()
                    .addItem(GfxBindingSetItem::Texture_SRV(1, hdr_color->getRHIHandle().Get()))
                    .addItem(GfxBindingSetItem::Sampler(9, m_sampler.Get())),
                m_binding_layout.Get());
            cutie::GraphicsState graphics_state;
            graphics_state.setPipeline(m_tone_map_pipeline.Get());
            graphics_state.setFramebuffer(tone_map_framebuffer);
            graphics_state.setViewport(viewport_state);
            graphics_state.addBindingSet(binding_set.Get());
            m_command_list->setGraphicsState(graphics_state);
            RenderFrameCounters::Self().addDrawCall(1); m_command_list->draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
        }

        // FXAA: LDR (tone mapped) -> LDR (anti-aliased)
        m_command_list->setTextureState(tone_map_color->getRHI(), cutie::AllSubresources, cutie::ResourceStates::ShaderResource);
        m_command_list->setTextureState(fxaa_color->getRHI(), cutie::AllSubresources, cutie::ResourceStates::RenderTarget);
        RenderFrameCounters::Self().addBarrier(); m_command_list->commitBarriers();
        m_command_list->clearTextureFloat(fxaa_color->getRHI(), cutie::AllSubresources, cutie::Color(0.0f, 0.0f, 0.0f, 1.0f));

        if (m_fxaa_pipeline) {
            auto binding_set = m_device->createBindingSet(
                GfxBindingSetDesc()
                    .addItem(GfxBindingSetItem::Texture_SRV(1, tone_map_color->getRHIHandle().Get()))
                    .addItem(GfxBindingSetItem::Sampler(9, m_sampler.Get())),
                m_binding_layout.Get());
            cutie::GraphicsState graphics_state;
            graphics_state.setPipeline(m_fxaa_pipeline.Get());
            graphics_state.setFramebuffer(fxaa_framebuffer);
            graphics_state.setViewport(viewport_state);
            graphics_state.addBindingSet(binding_set.Get());
            m_command_list->setGraphicsState(graphics_state);
            RenderFrameCounters::Self().addDrawCall(1); m_command_list->draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
        }

        m_command_list->setTextureState(fxaa_color->getRHI(), cutie::AllSubresources, cutie::ResourceStates::ShaderResource);
        RenderFrameCounters::Self().addBarrier(); m_command_list->commitBarriers();
    }

} // namespace dodoe
