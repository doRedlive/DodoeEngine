// do@Redlive

#include "baseline_outline_pass.h"

#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/render_service/binding_layout_cache.h"
#include "runtime/function/render/render_view/render_view.h"
#include "runtime/function/render/render_pipeline/render_pipeline_pass_utils.h"

namespace dodoe {

    Bool BaselineOutlinePass::initialize(const BaselinePassContext& context) {
        m_device = context.device;
        m_command_list = context.command_list;
        m_shader_library = context.shader_library;
        m_shared_render_service = context.shared_render_service;
        m_sampler = context.sampler;

        if (!m_shared_render_service || !m_shader_library) {
            return true;
        }
        auto* binding_layout_cache = m_shared_render_service->getBindingLayoutCache();
        DO_ASSERT(binding_layout_cache != nullptr, "BaselineOutlinePass: binding layout cache is unavailable");
        m_binding_layout = binding_layout_cache->getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::Pixel)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Pass))
                .addItem(GfxBindingLayoutItem::Texture_SRV(1))
                .addItem(GfxBindingLayoutItem::Sampler(9)));
        return true;
    }

    void BaselineOutlinePass::shutdown() {
        m_binding_layout = nullptr;
        m_pipeline = nullptr;
        m_sampler = nullptr;
        m_shader_library = nullptr;
        m_shared_render_service = nullptr;
        m_command_list = nullptr;
        m_device = nullptr;
    }

    void BaselineOutlinePass::ensurePipeline(const cutie::FramebufferInfo& framebuffer_info) {
        if (m_pipeline || !m_shader_library) {
            return;
        }
        const auto vertex_shader = m_shader_library->getFullscreenVertexShader();
        const auto pixel_shader = m_shader_library->getOutlinePixelShader();
        if (!vertex_shader || !pixel_shader) {
            DO_ERROR("BaselineOutlinePass: outline shaders are not loaded");
            return;
        }

        auto pipeline_desc = rendering_pipeline_utils::BuildFullscreenPipelineDesc(
            vertex_shader, pixel_shader, m_binding_layout, false, true);

        m_pipeline = m_device->createGraphicsPipeline(pipeline_desc, framebuffer_info);
        DO_INFO("BaselineOutlinePass: outline pipeline created");
    }

    void BaselineOutlinePass::render(RenderView& view, const Vector2i& extent,
                                     const GfxTextureHandle& selection_mask, cutie::IFramebuffer* framebuffer) {
        if (!m_pipeline || !selection_mask || !selection_mask->isGpuReady()) {
            return;
        }
        const auto viewport_state = rendering_pipeline_utils::BuildViewportState(view, extent);

        auto binding_set = m_device->createBindingSet(
            GfxBindingSetDesc()
                .addItem(GfxBindingSetItem::Texture_SRV(1, selection_mask->getRHIHandle().Get()))
                .addItem(GfxBindingSetItem::Sampler(9, m_sampler.Get())),
            m_binding_layout.Get());

        cutie::GraphicsState graphics_state;
        graphics_state.setPipeline(m_pipeline.Get());
        graphics_state.setFramebuffer(framebuffer);
        graphics_state.setViewport(viewport_state);
        graphics_state.addBindingSet(binding_set.Get());
        m_command_list->setGraphicsState(graphics_state);
        m_command_list->draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
    }

} // namespace dodoe
