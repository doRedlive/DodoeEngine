// do@Redlive

#include "baseline_renderer.h"

#include "runtime/function/render/render_view/render_view_family.h"
#include "runtime/function/render/render_view/render_view.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/render_pipeline/render_pipeline_pass_utils.h"

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#endif

namespace dodoe {

    namespace {
        GfxTextureHandle CreateColorTarget(GfxDeviceHandle device, UInt32 width, UInt32 height,
                                           GfxFormat format, const char* name) {
            GfxTextureDesc desc;
            desc.setDimension(GfxTextureDimension::Texture2D)
                .setFormat(format)
                .setWidth(width)
                .setHeight(height)
                .setIsRenderTarget(true)
                .enableAutomaticStateTracking(GfxResourceStates::RenderTarget)
                .setDebugName(name);
            auto texture = create_ref<GfxTexture>(desc, name);
            texture->initializeGpu(device);
            return texture;
        }

        GfxTextureHandle CreateDepthTarget(GfxDeviceHandle device, UInt32 width, UInt32 height,
                                           const char* name) {
            GfxTextureDesc desc;
            desc.setDimension(GfxTextureDimension::Texture2D)
                .setFormat(GfxFormat::D32)
                .setWidth(width)
                .setHeight(height)
                .setIsRenderTarget(true)
                .enableAutomaticStateTracking(GfxResourceStates::DepthWrite)
                .setDebugName(name);
            auto texture = create_ref<GfxTexture>(desc, name);
            texture->initializeGpu(device);
            return texture;
        }

        GfxFramebufferHandle CreateFramebuffer(GfxDeviceHandle device,
                                               const DynamicArray<GfxTextureHandle>& colors,
                                               const GfxTextureHandle& depth = {}) {
            GfxFramebufferDesc desc;
            for (const auto& color : colors) {
                desc.addColorAttachment(color);
            }
            if (depth) {
                desc.setDepthAttachment(depth);
            }
            auto framebuffer = create_ref<GfxFramebuffer>(desc);
            framebuffer->initializeGpu(device);
            return framebuffer;
        }
    }

    Bool BaselineRenderer::initialize(const BaselineRendererCreateInfo& info) {
        if (!info.device) {
            DO_ERROR("BaselineRenderer: device is null");
            return false;
        }
        m_device = info.device;
        m_command_list = m_device->createCommandList();
        if (!m_command_list) {
            DO_ERROR("BaselineRenderer: failed to create raw command list");
            return false;
        }

        GfxSamplerDesc sampler_desc;
        sampler_desc.setAllFilters(true)
            .setAllAddressModes(GfxSamplerAddressMode::Clamp);
        m_sampler = m_device->createSampler(sampler_desc);

        BaselinePassContext context;
        context.device = m_device;
        context.command_list = m_command_list;
        context.shader_library = info.shader_library;
        context.shared_render_service = info.shared_render_service;
        context.sampler = m_sampler;

        m_gbuffer_pass = create_scope<BaselineGBufferPass>();
        if (!m_gbuffer_pass->initialize(context)) {
            return false;
        }
        m_lighting_pass = create_scope<BaselineLightingPass>();
        if (!m_lighting_pass->initialize(context)) {
            return false;
        }
        m_sky_pass = create_scope<BaselineSkyPass>();
        if (!m_sky_pass->initialize(context)) {
            return false;
        }
        m_sprite_pass = create_scope<BaselineSpritePass>();
        if (!m_sprite_pass->initialize(context)) {
            return false;
        }
#ifdef DODOE_DEBUG_ENABLED
        m_imgui_pass = create_scope<BaselineImGuiPass>();
        if (!m_imgui_pass->initialize(context)) {
            return false;
        }
#endif
        m_post_process_pass = create_scope<BaselinePostProcessPass>();
        if (!m_post_process_pass->initialize(context)) {
            return false;
        }
        m_present_pass = create_scope<BaselinePresentPass>();
        if (!m_present_pass->initialize(context)) {
            return false;
        }
#ifdef DODOE_DEBUG_ENABLED
        m_present_pass->setImGuiPass(m_imgui_pass.get());
#endif

        m_frame_counter = 0;
        DO_INFO("BaselineRenderer: initialized (raw cutie path, deferred GBuffer + lighting + AA)");
        return true;
    }

    Bool BaselineRenderer::ensureRenderTarget(const Vector2i& extent) {
        if (m_rt.gbuffer_framebuffer && m_scene_rt_extent == extent &&
            m_rt.gbuffer_framebuffer->isGpuReady() &&
            m_rt.scene_color && m_rt.scene_color->isGpuReady() &&
            m_rt.fxaa_color && m_rt.fxaa_color->isGpuReady()) {
            return true;
        }
        if (extent.x <= 0 || extent.y <= 0) {
            return false;
        }

        m_rt = BaselineRenderTargets{};
        const UInt32 width = static_cast<UInt32>(extent.x);
        const UInt32 height = static_cast<UInt32>(extent.y);

        // GBuffer
        m_rt.gbuffer_albedo = CreateColorTarget(m_device, width, height, GfxFormat::RGBA8_UNORM, "BaselineGBufferAlbedo");
        m_rt.gbuffer_normal = CreateColorTarget(m_device, width, height, GfxFormat::RGBA16_FLOAT, "BaselineGBufferNormal");
        m_rt.gbuffer_position = CreateColorTarget(m_device, width, height, GfxFormat::RGBA32_FLOAT, "BaselineGBufferPosition");
        m_rt.gbuffer_material = CreateColorTarget(m_device, width, height, GfxFormat::RGBA8_UNORM, "BaselineGBufferMaterial");
        m_rt.gbuffer_depth = CreateDepthTarget(m_device, width, height, "BaselineGBufferDepth");
        m_rt.gbuffer_framebuffer = CreateFramebuffer(m_device,
            {m_rt.gbuffer_albedo, m_rt.gbuffer_normal, m_rt.gbuffer_position, m_rt.gbuffer_material},
            m_rt.gbuffer_depth);

        // HDR scene color
        m_rt.scene_color = CreateColorTarget(m_device, width, height, GfxFormat::RGBA16_FLOAT, "BaselineSceneColor");
        m_rt.lighting_framebuffer = CreateFramebuffer(m_device, {m_rt.scene_color});
        m_rt.sprite_framebuffer = CreateFramebuffer(m_device, {m_rt.scene_color}, m_rt.gbuffer_depth);

        // LDR post-process ping-pong
        m_rt.tone_map_color = CreateColorTarget(m_device, width, height, GfxFormat::RGBA8_UNORM, "BaselineToneMapColor");
        m_rt.fxaa_color = CreateColorTarget(m_device, width, height, GfxFormat::RGBA8_UNORM, "BaselineFxaaColor");
        m_rt.tone_map_framebuffer = CreateFramebuffer(m_device, {m_rt.tone_map_color});
        m_rt.fxaa_framebuffer = CreateFramebuffer(m_device, {m_rt.fxaa_color});

        m_scene_rt_extent = extent;
        return true;
    }

    void BaselineRenderer::shutdown() {
        if (m_device) {
            m_device->waitForIdle();
        }
        m_gbuffer_pass.reset();
        m_lighting_pass.reset();
        m_sky_pass.reset();
        m_sprite_pass.reset();
#ifdef DODOE_DEBUG_ENABLED
        m_imgui_pass.reset();
#endif
        m_post_process_pass.reset();
        m_present_pass.reset();
        m_rt = BaselineRenderTargets{};
        m_scene_rt_extent = Vector2i(0, 0);
        m_sampler = nullptr;
        m_command_list = nullptr;
        m_device = nullptr;
        m_frame_counter = 0;
    }

    void BaselineRenderer::render(GfxContext& gfx, UInt32 swapchain_image_index,
                                  RenderViewFamily& view_family, RenderScene& scene) {
        if (!m_device || !m_command_list) {
            DO_ERROR("BaselineRenderer: render skipped, device or command list is null");
            return;
        }

        const auto framebuffer = gfx.getSwapchainFramebuffer(swapchain_image_index);
        if (!framebuffer || !framebuffer->isGpuReady()) {
            DO_ERROR("BaselineRenderer: swapchain framebuffer is unavailable, index={}", swapchain_image_index);
            return;
        }

        const auto extent = gfx.getSwapchainExtent2D();
        if (!ensureRenderTarget(extent)) {
            return;
        }

        m_gbuffer_pass->ensurePipeline(m_rt.gbuffer_framebuffer->getFramebufferInfo().getRHI());
        m_lighting_pass->ensurePipeline(m_rt.lighting_framebuffer->getFramebufferInfo().getRHI());
        m_sky_pass->ensurePipeline(m_rt.lighting_framebuffer->getFramebufferInfo().getRHI());
        m_sprite_pass->ensurePipeline(m_rt.sprite_framebuffer->getFramebufferInfo().getRHI());
        m_post_process_pass->ensurePipelines(m_rt.tone_map_framebuffer->getFramebufferInfo().getRHI());
#ifdef DODOE_DEBUG_ENABLED
        m_imgui_pass->ensurePipeline(framebuffer->getFramebufferInfo().getRHI());
#endif
        m_present_pass->ensurePipeline(framebuffer->getFramebufferInfo().getRHI());

        m_command_list->open();

        for (auto& view : view_family.getViews()) {
            const auto viewport_state = rendering_pipeline_utils::BuildViewportState(view, extent);
            m_gbuffer_pass->setupView(view, view_family);

            // GBuffer pass
            m_command_list->setTextureState(m_rt.gbuffer_albedo->getRHI(), cutie::AllSubresources, cutie::ResourceStates::RenderTarget);
            m_command_list->setTextureState(m_rt.gbuffer_normal->getRHI(), cutie::AllSubresources, cutie::ResourceStates::RenderTarget);
            m_command_list->setTextureState(m_rt.gbuffer_position->getRHI(), cutie::AllSubresources, cutie::ResourceStates::RenderTarget);
            m_command_list->setTextureState(m_rt.gbuffer_material->getRHI(), cutie::AllSubresources, cutie::ResourceStates::RenderTarget);
            m_command_list->setTextureState(m_rt.gbuffer_depth->getRHI(), cutie::AllSubresources, cutie::ResourceStates::DepthWrite);
            m_command_list->commitBarriers();
            m_command_list->clearTextureFloat(m_rt.gbuffer_albedo->getRHI(), cutie::AllSubresources, cutie::Color(0.08f, 0.09f, 0.11f, 1.0f));
            m_command_list->clearTextureFloat(m_rt.gbuffer_normal->getRHI(), cutie::AllSubresources, cutie::Color(0.0f, 0.0f, 0.0f, 1.0f));
            m_command_list->clearTextureFloat(m_rt.gbuffer_position->getRHI(), cutie::AllSubresources, cutie::Color(0.0f, 0.0f, 0.0f, 1.0f));
            m_command_list->clearTextureFloat(m_rt.gbuffer_material->getRHI(), cutie::AllSubresources, cutie::Color(0.0f, 1.0f, 1.0f, 1.0f));
            m_command_list->clearDepthStencilTexture(m_rt.gbuffer_depth->getRHI(), cutie::AllSubresources, true, 1.0f, false, 0);
            m_gbuffer_pass->render(view, scene, viewport_state, m_rt.gbuffer_framebuffer->getRHI());

            m_command_list->setTextureState(m_rt.gbuffer_albedo->getRHI(), cutie::AllSubresources, cutie::ResourceStates::ShaderResource);
            m_command_list->setTextureState(m_rt.gbuffer_normal->getRHI(), cutie::AllSubresources, cutie::ResourceStates::ShaderResource);
            m_command_list->setTextureState(m_rt.gbuffer_position->getRHI(), cutie::AllSubresources, cutie::ResourceStates::ShaderResource);
            m_command_list->setTextureState(m_rt.gbuffer_material->getRHI(), cutie::AllSubresources, cutie::ResourceStates::ShaderResource);
            m_command_list->commitBarriers();

            // Lighting pass
            m_command_list->setTextureState(m_rt.scene_color->getRHI(), cutie::AllSubresources, cutie::ResourceStates::RenderTarget);
            m_command_list->commitBarriers();
            m_command_list->clearTextureFloat(m_rt.scene_color->getRHI(), cutie::AllSubresources, cutie::Color(0.0f, 0.0f, 0.0f, 1.0f));
            m_lighting_pass->render(view, scene, viewport_state, m_rt.lighting_framebuffer->getRHI(),
                m_rt.gbuffer_albedo, m_rt.gbuffer_normal, m_rt.gbuffer_position, m_rt.gbuffer_material);

            // Sky pass
            m_command_list->setTextureState(m_rt.gbuffer_depth->getRHI(), cutie::AllSubresources, cutie::ResourceStates::ShaderResource);
            m_command_list->commitBarriers();
            m_sky_pass->render(view, scene, viewport_state, m_rt.lighting_framebuffer->getRHI(), m_rt.gbuffer_depth);

            // Sprite pass
            m_command_list->setTextureState(m_rt.gbuffer_depth->getRHI(), cutie::AllSubresources, cutie::ResourceStates::DepthRead);
            m_command_list->commitBarriers();
            m_sprite_pass->render(view, scene, m_rt.sprite_framebuffer->getRHI(), viewport_state);

            // Post-process pass
            m_command_list->setTextureState(m_rt.scene_color->getRHI(), cutie::AllSubresources, cutie::ResourceStates::ShaderResource);
            m_command_list->commitBarriers();
            m_post_process_pass->render(view, extent, m_rt.scene_color,
                m_rt.tone_map_color, m_rt.tone_map_framebuffer->getRHI(),
                m_rt.fxaa_color, m_rt.fxaa_framebuffer->getRHI());
        }

        m_present_pass->render(gfx, swapchain_image_index, extent, m_rt.fxaa_color);

        m_command_list->close();
        m_device->executeCommandList(m_command_list.Get());
        m_device->runGarbageCollection();

        if (((m_frame_counter++) % 120) == 0) {
#if defined(_WIN32)
            PROCESS_MEMORY_COUNTERS_EX pmc{};
            K32GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc));
            DO_INFO("BaselineRenderer: frame={} private_commit={:.1f} MB",
                m_frame_counter - 1,
                static_cast<double>(pmc.PrivateUsage) / (1024.0 * 1024.0));
#else
            DO_INFO("BaselineRenderer: frame={}", m_frame_counter - 1);
#endif
        }
    }

} // namespace dodoe
