// do@Redlive

#include "baseline_present_pass.h"

#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/render/render_frame/frame_telemetry.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/render_service/binding_layout_cache.h"

#ifdef DODOE_DEBUG_ENABLED
#include "baseline_imgui_pass.h"
#endif

namespace dodoe {

    Bool BaselinePresentPass::initialize(const BaselinePassContext& context) {
        m_device = context.device;
        m_command_list = context.command_list;
        m_shader_library = context.shader_library;
        m_shared_render_service = context.shared_render_service;
        m_sampler = context.sampler;

        if (!m_shared_render_service || !m_shader_library) {
            return true;
        }
        auto* binding_layout_cache = m_shared_render_service->getBindingLayoutCache();
        DO_ASSERT(binding_layout_cache != nullptr, "BaselinePresentPass: binding layout cache is unavailable");
        m_binding_layout = binding_layout_cache->getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::Pixel)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Pass))
                .addItem(GfxBindingLayoutItem::Texture_SRV(1))
                .addItem(GfxBindingLayoutItem::Sampler(9)));
        return true;
    }

    void BaselinePresentPass::shutdown() {
        m_pipeline = nullptr;
        m_binding_layout = nullptr;
        m_sampler = nullptr;
        m_shader_library = nullptr;
        m_shared_render_service = nullptr;
        m_command_list = nullptr;
        m_device = nullptr;
    }

    void BaselinePresentPass::setImGuiPass(BaselineImGuiPass* imgui_pass) {
#ifdef DODOE_DEBUG_ENABLED
        m_imgui_pass = imgui_pass;
#else
        (void)imgui_pass;
#endif
    }

    void BaselinePresentPass::ensurePipeline(const cutie::FramebufferInfo& framebuffer_info) {        if (m_pipeline) {
            return;
        }
        if (!m_shader_library) {
            return;
        }
        const auto vertex_shader = m_shader_library->getFullscreenVertexShader();
        const auto pixel_shader = m_shader_library->getBaselinePixelShader();
        if (!vertex_shader || !pixel_shader) {
            DO_ERROR("BaselinePresentPass: fullscreen shaders are not loaded");
            return;
        }

        GfxGraphicsPipelineDesc pipeline_desc;
        pipeline_desc.setPrimType(GfxPrimitiveType::TriangleList);
        pipeline_desc.setVertexShader(vertex_shader.Get());
        pipeline_desc.setPixelShader(pixel_shader.Get());
        pipeline_desc.addBindingLayout(m_binding_layout.Get());

        GfxDepthStencilState depth_stencil_state;
        depth_stencil_state.disableDepthTest().disableDepthWrite().disableStencil();
        GfxRasterState raster_state;
        raster_state.setCullNone();
        GfxRenderState render_state;
        render_state.setDepthStencilState(depth_stencil_state)
            .setRasterState(raster_state);
        pipeline_desc.setRenderState(render_state);

        m_pipeline = m_device->createGraphicsPipeline(pipeline_desc, framebuffer_info);
        DO_INFO("BaselinePresentPass: render pipeline created");
    }

    void BaselinePresentPass::render(GfxContext& gfx, UInt32 swapchain_image_index,
                                     const Vector2i& extent, const GfxTextureHandle& scene_color) {
        const auto framebuffer = gfx.getSwapchainFramebuffer(swapchain_image_index);
        if (!framebuffer || !framebuffer->isGpuReady()) {
            DO_ERROR("BaselinePresentPass: swapchain framebuffer is unavailable, index={}", swapchain_image_index);
            return;
        }

        const auto& textures = gfx.getSwapchainTextures();
        cutie::ITexture* color_attachment = nullptr;
        if (swapchain_image_index < textures.size() && textures[swapchain_image_index] &&
            textures[swapchain_image_index]->isGpuReady()) {
            color_attachment = textures[swapchain_image_index]->getRHI();
        }

        m_command_list->setTextureState(scene_color->getRHI(), cutie::AllSubresources, cutie::ResourceStates::ShaderResource);
        if (color_attachment) {
            m_command_list->setTextureState(color_attachment, cutie::AllSubresources, cutie::ResourceStates::RenderTarget);
        }
        RenderFrameCounters::Self().addBarrier(); m_command_list->commitBarriers();

        if (color_attachment && m_pipeline) {
            auto present_binding_set = m_device->createBindingSet(
                GfxBindingSetDesc()
                    .addItem(GfxBindingSetItem::Texture_SRV(1, scene_color->getRHIHandle().Get()))
                    .addItem(GfxBindingSetItem::Sampler(9, m_sampler.Get())),
                m_binding_layout.Get());
            GfxViewportState present_viewport;
            present_viewport.addViewportAndScissorRect(GfxViewport(
                0.0f, static_cast<Float>(extent.x),
                0.0f, static_cast<Float>(extent.y),
                0.0f, 1.0f));
            cutie::GraphicsState present_state;
            present_state.setPipeline(m_pipeline.Get());
            present_state.setFramebuffer(framebuffer->getRHI());
            present_state.setViewport(present_viewport);
            present_state.addBindingSet(present_binding_set.Get());
            m_command_list->setGraphicsState(present_state);
            RenderFrameCounters::Self().addDrawCall(1); m_command_list->draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
        }

#ifdef DODOE_DEBUG_ENABLED
        if (m_imgui_pass) {
            m_imgui_pass->render(framebuffer->getRHI());
        }
#endif

        if (color_attachment) {
            m_command_list->setTextureState(color_attachment, cutie::AllSubresources, cutie::ResourceStates::Present);
        }
        RenderFrameCounters::Self().addBarrier(); m_command_list->commitBarriers();
    }

} // namespace dodoe
