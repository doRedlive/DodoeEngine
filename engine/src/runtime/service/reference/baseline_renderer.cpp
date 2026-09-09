// do@Redlive

#include "baseline_renderer.h"

#include "runtime/function/render/render_view/render_view_family.h"
#include "runtime/function/render/render_view/render_view.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/render_pipeline/render_pipeline_pass_utils.h"
#include "runtime/function/render/render_settings.h"
#include "runtime/function/render/render_frame/frame_telemetry.h"
#ifdef DODOE_DEBUG_ENABLED
#include "runtime/service/debug/debug_imgui.h"
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
        m_shader_library = info.shader_library;
        m_shared_render_service = info.shared_render_service;
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
        m_shadow_pass = create_scope<BaselineShadowPass>();
        if (!m_shadow_pass->initialize(context)) {
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
        m_outline_pass = create_scope<BaselineOutlinePass>();
        if (!m_outline_pass->initialize(context)) {
            return false;
        }
#ifdef DODOE_DEBUG_ENABLED
        m_imgui_pass = create_scope<BaselineImGuiPass>();
        if (!m_imgui_pass->initialize(context)) {
            return false;
        }
        m_pick_pass = create_scope<BaselinePickPass>();
        if (!m_pick_pass->initialize(context)) {
            return false;
        }
#endif
        m_post_process_pass = create_scope<BaselinePostProcessPass>();
        if (!m_post_process_pass->initialize(context)) {
            return false;
        }
        if (RenderSettings::IsTaaEnabled()) {
            m_taa_pass = create_scope<BaselineTaaPass>();
            if (!m_taa_pass->initialize(context)) {
                return false;
            }
        }
        m_present_pass = create_scope<BaselinePresentPass>();
        if (!m_present_pass->initialize(context)) {
            return false;
        }
#ifdef DODOE_DEBUG_ENABLED
        m_present_pass->setImGuiPass(m_imgui_pass.get());
#endif

        DO_INFO("BaselineRenderer: initialized (raw cutie path, deferred GBuffer + lighting + AA)");
        return true;
    }

    Bool BaselineRenderer::ensureRenderTarget(const Vector2i& extent) {
        if (m_rt.gbuffer_framebuffer && m_scene_rt_extent == extent &&
            m_rt.gbuffer_framebuffer->isGpuReady() &&
            m_rt.gbuffer_motion && m_rt.gbuffer_motion->isGpuReady() &&
            m_rt.scene_color && m_rt.scene_color->isGpuReady() &&
            m_rt.fxaa_color && m_rt.fxaa_color->isGpuReady() &&
            (!m_taa_pass || (m_rt.taa_history_a && m_rt.taa_history_a->isGpuReady() &&
                m_rt.taa_history_b && m_rt.taa_history_b->isGpuReady() &&
                m_rt.taa_framebuffer_a && m_rt.taa_framebuffer_b)) &&
            m_rt.pick_id && m_rt.pick_id->isGpuReady() &&
            m_rt.pick_framebuffer && m_rt.pick_framebuffer->isGpuReady() &&
            m_pick_staging) {
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
        m_rt.gbuffer_emissive = CreateColorTarget(m_device, width, height, GfxFormat::RGBA16_FLOAT, "BaselineGBufferEmissive");
        m_rt.gbuffer_motion = CreateColorTarget(m_device, width, height, GfxFormat::RG16_FLOAT, "BaselineGBufferMotion");
        m_rt.gbuffer_depth = CreateDepthTarget(m_device, width, height, "BaselineGBufferDepth");
        m_rt.gbuffer_framebuffer = CreateFramebuffer(m_device,
            {m_rt.gbuffer_albedo, m_rt.gbuffer_normal, m_rt.gbuffer_position, m_rt.gbuffer_material, m_rt.gbuffer_emissive, m_rt.gbuffer_motion},
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

        if (m_taa_pass) {
            m_rt.taa_history_a = CreateColorTarget(m_device, width, height, GfxFormat::RGBA16_FLOAT, "BaselineTaaHistoryA");
            m_rt.taa_history_b = CreateColorTarget(m_device, width, height, GfxFormat::RGBA16_FLOAT, "BaselineTaaHistoryB");
            m_rt.taa_framebuffer_a = CreateFramebuffer(m_device, {m_rt.taa_history_a});
            m_rt.taa_framebuffer_b = CreateFramebuffer(m_device, {m_rt.taa_history_b});
            m_taa_pass->shutdown();
            if (!m_taa_pass->initialize({m_device, m_command_list, m_shader_library,
                    m_shared_render_service, m_sampler})) {
                return false;
            }
        }

        // Viewport pick id target
        m_rt.pick_id = CreateColorTarget(m_device, width, height, GfxFormat::R32_UINT, "BaselinePickId");
        m_rt.pick_framebuffer = CreateFramebuffer(m_device, {m_rt.pick_id}, m_rt.gbuffer_depth);
        GfxTextureDesc pick_staging_desc;
        pick_staging_desc.setDimension(GfxTextureDimension::Texture2D)
            .setFormat(GfxFormat::R32_UINT)
            .setWidth(1)
            .setHeight(1);
        m_pick_staging = m_device->createStagingTexture(pick_staging_desc, GfxCpuAccessMode::Read);

        m_scene_rt_extent = extent;
        m_rt_recreated = true;
        return true;
    }

    void BaselineRenderer::shutdown() {
        if (m_device) {
            m_device->waitForIdle();
        }
        GpuCulling::Destroy(m_gpu_culling);
        m_gbuffer_pass.reset();
        m_lighting_pass.reset();
        m_shadow_pass.reset();
        m_sky_pass.reset();
        m_sprite_pass.reset();
        m_outline_pass.reset();
#ifdef DODOE_DEBUG_ENABLED
        m_imgui_pass.reset();
        m_pick_pass.reset();
#endif
        m_pick_staging = nullptr;
        m_pick_ids.clear();
        m_post_process_pass.reset();
        m_taa_pass.reset();
        m_present_pass.reset();
        m_rt = BaselineRenderTargets{};
        m_scene_rt_extent = Vector2i(0, 0);
        m_rt_recreated = false;
        m_taa_frame_index = 0;
        m_sampler = nullptr;
        m_command_list = nullptr;
        m_shared_render_service = nullptr;
        m_device = nullptr;
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

        if (RenderSettings::IsGpuDrivenSupported() && !m_gpu_culling) {
            m_gpu_culling = GpuCulling::Create({&gfx, const_cast<ShaderLibrary*>(m_shader_library)});
        }
        m_gbuffer_pass->setGpuCulling(m_gpu_culling.get());

        m_gbuffer_pass->ensurePipeline(m_rt.gbuffer_framebuffer->getFramebufferInfo().getRHI());
        m_lighting_pass->ensurePipeline(m_rt.lighting_framebuffer->getFramebufferInfo().getRHI());
        m_shadow_pass->ensurePipeline();
        m_sky_pass->ensurePipeline(m_rt.lighting_framebuffer->getFramebufferInfo().getRHI());
        m_sprite_pass->ensurePipeline(m_rt.sprite_framebuffer->getFramebufferInfo().getRHI());
        m_outline_pass->ensurePipeline(m_rt.lighting_framebuffer->getFramebufferInfo().getRHI());
        m_post_process_pass->ensurePipelines(m_rt.tone_map_framebuffer->getFramebufferInfo().getRHI());
        if (m_taa_pass) {
            m_taa_pass->ensurePipeline(m_rt.taa_framebuffer_a->getFramebufferInfo().getRHI());
        }
#ifdef DODOE_DEBUG_ENABLED
        m_imgui_pass->ensurePipeline(framebuffer->getFramebufferInfo().getRHI());
        m_pick_pass->ensurePipeline(m_rt.pick_framebuffer->getFramebufferInfo().getRHI());
#endif
        m_present_pass->ensurePipeline(framebuffer->getFramebufferInfo().getRHI());

#ifdef DODOE_DEBUG_ENABLED
        Int32 pick_x = -1;
        Int32 pick_y = -1;
        Bool pick_requested = DebugImGui::ConsumePickRequest(pick_x, pick_y);
        if (pick_x < 0 || pick_y < 0 || pick_x >= static_cast<Int32>(extent.x) || pick_y >= static_cast<Int32>(extent.y)) {
            pick_requested = false;
        }
#endif

        m_command_list->open();

        const Bool taa_targets_recreated = m_rt_recreated;
        if (taa_targets_recreated) {
            m_gbuffer_pass->resetMotionHistory();
        }
        m_rt_recreated = false;

#ifdef DODOE_DEBUG_ENABLED
        Bool pick_copied = false;
#endif
        for (auto& view : view_family.getViews()) {
            const auto viewport_state = rendering_pipeline_utils::BuildViewportState(view, extent);
            m_gbuffer_pass->setupView(view, view_family);

            // GBuffer pass
            m_command_list->beginMarker("Baseline.GBuffer");
            m_command_list->setTextureState(m_rt.gbuffer_albedo->getRHI(), cutie::AllSubresources, cutie::ResourceStates::RenderTarget);
            m_command_list->setTextureState(m_rt.gbuffer_normal->getRHI(), cutie::AllSubresources, cutie::ResourceStates::RenderTarget);
            m_command_list->setTextureState(m_rt.gbuffer_position->getRHI(), cutie::AllSubresources, cutie::ResourceStates::RenderTarget);
            m_command_list->setTextureState(m_rt.gbuffer_material->getRHI(), cutie::AllSubresources, cutie::ResourceStates::RenderTarget);
            m_command_list->setTextureState(m_rt.gbuffer_emissive->getRHI(), cutie::AllSubresources, cutie::ResourceStates::RenderTarget);
            m_command_list->setTextureState(m_rt.gbuffer_motion->getRHI(), cutie::AllSubresources, cutie::ResourceStates::RenderTarget);
            m_command_list->setTextureState(m_rt.gbuffer_depth->getRHI(), cutie::AllSubresources, cutie::ResourceStates::DepthWrite);
            RenderFrameCounters::Self().addBarrier(); m_command_list->commitBarriers();
            m_command_list->clearTextureFloat(m_rt.gbuffer_albedo->getRHI(), cutie::AllSubresources, cutie::Color(0.08f, 0.09f, 0.11f, 1.0f));
            m_command_list->clearTextureFloat(m_rt.gbuffer_normal->getRHI(), cutie::AllSubresources, cutie::Color(0.0f, 0.0f, 0.0f, 1.0f));
            m_command_list->clearTextureFloat(m_rt.gbuffer_position->getRHI(), cutie::AllSubresources, cutie::Color(0.0f, 0.0f, 0.0f, 1.0f));
            m_command_list->clearTextureFloat(m_rt.gbuffer_material->getRHI(), cutie::AllSubresources, cutie::Color(0.0f, 1.0f, 1.0f, 0.0f));
            m_command_list->clearTextureFloat(m_rt.gbuffer_emissive->getRHI(), cutie::AllSubresources, cutie::Color(0.0f, 0.0f, 0.0f, 1.0f));
            m_command_list->clearTextureFloat(m_rt.gbuffer_motion->getRHI(), cutie::AllSubresources, cutie::Color(0.0f, 0.0f, 0.0f, 1.0f));
            m_command_list->clearDepthStencilTexture(m_rt.gbuffer_depth->getRHI(), cutie::AllSubresources, true, 1.0f, false, 0);
            m_gbuffer_pass->render(view, scene, viewport_state, m_rt.gbuffer_framebuffer->getRHI());

            m_command_list->setTextureState(m_rt.gbuffer_albedo->getRHI(), cutie::AllSubresources, cutie::ResourceStates::ShaderResource);
            m_command_list->setTextureState(m_rt.gbuffer_normal->getRHI(), cutie::AllSubresources, cutie::ResourceStates::ShaderResource);
            m_command_list->setTextureState(m_rt.gbuffer_position->getRHI(), cutie::AllSubresources, cutie::ResourceStates::ShaderResource);
            m_command_list->setTextureState(m_rt.gbuffer_material->getRHI(), cutie::AllSubresources, cutie::ResourceStates::ShaderResource);
            m_command_list->setTextureState(m_rt.gbuffer_emissive->getRHI(), cutie::AllSubresources, cutie::ResourceStates::ShaderResource);
            m_command_list->setTextureState(m_rt.gbuffer_motion->getRHI(), cutie::AllSubresources, cutie::ResourceStates::ShaderResource);
            RenderFrameCounters::Self().addBarrier(); m_command_list->commitBarriers();
            m_command_list->endMarker();

            // Viewport pick pass
#ifdef DODOE_DEBUG_ENABLED
            m_pick_ids.clear();
            if (pick_requested) {
                m_command_list->beginMarker("Baseline.Pick");
                m_command_list->setTextureState(m_rt.pick_id->getRHI(), cutie::AllSubresources, cutie::ResourceStates::RenderTarget);
                m_command_list->setTextureState(m_rt.gbuffer_depth->getRHI(), cutie::AllSubresources, cutie::ResourceStates::DepthRead);
                RenderFrameCounters::Self().addBarrier(); m_command_list->commitBarriers();
                m_command_list->clearTextureUInt(m_rt.pick_id->getRHI(), cutie::AllSubresources, 0);
                if (m_pick_pass->render(view, viewport_state, m_rt.pick_framebuffer->getRHI(),
                        m_gbuffer_pass->getInstanceBuffer(), m_pick_ids)) {
                    m_command_list->setTextureState(m_rt.pick_id->getRHI(), cutie::AllSubresources, cutie::ResourceStates::CopySource);
                    RenderFrameCounters::Self().addBarrier(); m_command_list->commitBarriers();
                    const cutie::TextureSlice src_slice = cutie::TextureSlice()
                        .setOrigin(static_cast<UInt32>(pick_x), static_cast<UInt32>(pick_y))
                        .setSize(1, 1);
                    m_command_list->copyTexture(m_pick_staging.Get(), cutie::TextureSlice().setSize(1, 1),
                        m_rt.pick_id->getRHI(), src_slice);
                    pick_copied = true;
                }
                m_command_list->endMarker();
            }
#endif

            // Shadow pass
            m_command_list->beginMarker("Baseline.Shadow");
            const BaselineShadowResult shadow = m_shadow_pass->render(view, scene);
            m_command_list->endMarker();

            // Lighting pass
            m_command_list->beginMarker("Baseline.Lighting");
            m_command_list->setTextureState(m_rt.scene_color->getRHI(), cutie::AllSubresources, cutie::ResourceStates::RenderTarget);
            RenderFrameCounters::Self().addBarrier(); m_command_list->commitBarriers();
            m_command_list->clearTextureFloat(m_rt.scene_color->getRHI(), cutie::AllSubresources, cutie::Color(0.0f, 0.0f, 0.0f, 1.0f));
            m_lighting_pass->render(view, scene, viewport_state, m_rt.lighting_framebuffer->getRHI(),
                m_rt.gbuffer_albedo, m_rt.gbuffer_normal, m_rt.gbuffer_position, m_rt.gbuffer_material,
                m_rt.gbuffer_emissive, shadow);
            m_command_list->endMarker();

            // Sky pass
            m_command_list->beginMarker("Baseline.Sky");
            m_command_list->setTextureState(m_rt.gbuffer_depth->getRHI(), cutie::AllSubresources, cutie::ResourceStates::ShaderResource);
            RenderFrameCounters::Self().addBarrier(); m_command_list->commitBarriers();
            m_sky_pass->render(view, scene, viewport_state, m_rt.lighting_framebuffer->getRHI(), m_rt.gbuffer_depth);
            m_command_list->endMarker();

            // Sprite pass
            m_command_list->beginMarker("Baseline.Sprite");
            m_command_list->setTextureState(m_rt.gbuffer_depth->getRHI(), cutie::AllSubresources, cutie::ResourceStates::DepthRead);
            RenderFrameCounters::Self().addBarrier(); m_command_list->commitBarriers();
            m_sprite_pass->render(view, scene, m_rt.sprite_framebuffer->getRHI(), viewport_state);
            m_command_list->endMarker();

            // Selection outline pass (dilated selection mask, blended onto scene color)
            m_command_list->beginMarker("Baseline.Outline");
            m_command_list->setTextureState(m_rt.scene_color->getRHI(), cutie::AllSubresources, cutie::ResourceStates::RenderTarget);
            RenderFrameCounters::Self().addBarrier(); m_command_list->commitBarriers();
            m_outline_pass->render(view, extent, m_rt.gbuffer_material, m_rt.lighting_framebuffer->getRHI());
            m_command_list->endMarker();

            GfxTextureHandle aa_input = m_rt.scene_color;

            // TAA resolve (HDR accumulation over history)
            if (m_taa_pass && m_rt.taa_history_a && m_rt.taa_history_b) {
                m_command_list->beginMarker("Baseline.TAA");
                ++m_taa_frame_index;
                const Bool taa_flip = (m_taa_frame_index & 1ull) != 0ull;
                const GfxTextureHandle taa_read = taa_flip ? m_rt.taa_history_b : m_rt.taa_history_a;
                const GfxTextureHandle taa_write = taa_flip ? m_rt.taa_history_a : m_rt.taa_history_b;
                cutie::IFramebuffer* taa_write_framebuffer = taa_flip
                    ? m_rt.taa_framebuffer_a->getRHI()
                    : m_rt.taa_framebuffer_b->getRHI();
                m_command_list->setTextureState(m_rt.scene_color->getRHI(), cutie::AllSubresources, cutie::ResourceStates::ShaderResource);
                m_command_list->setTextureState(taa_read->getRHI(), cutie::AllSubresources, cutie::ResourceStates::ShaderResource);
                m_command_list->setTextureState(taa_write->getRHI(), cutie::AllSubresources, cutie::ResourceStates::RenderTarget);
                RenderFrameCounters::Self().addBarrier(); m_command_list->commitBarriers();
                m_taa_pass->render(view, extent, m_rt.scene_color, m_rt.gbuffer_position,
                    m_rt.gbuffer_depth, m_rt.gbuffer_motion, taa_read, taa_write,
                    taa_write_framebuffer, viewport_state, taa_targets_recreated);
                m_command_list->setTextureState(taa_write->getRHI(), cutie::AllSubresources, cutie::ResourceStates::ShaderResource);
                RenderFrameCounters::Self().addBarrier(); m_command_list->commitBarriers();
                aa_input = taa_write;
                m_command_list->endMarker();
            }

            // Post-process pass
            m_command_list->beginMarker("Baseline.PostProcess");
            m_command_list->setTextureState(m_rt.scene_color->getRHI(), cutie::AllSubresources, cutie::ResourceStates::ShaderResource);
            RenderFrameCounters::Self().addBarrier(); m_command_list->commitBarriers();
            m_post_process_pass->render(view, extent, aa_input,
                m_rt.tone_map_color, m_rt.tone_map_framebuffer->getRHI(),
                m_rt.fxaa_color, m_rt.fxaa_framebuffer->getRHI());
            m_command_list->endMarker();
        }

        m_command_list->beginMarker("Baseline.Present");
        m_present_pass->render(gfx, swapchain_image_index, extent, m_rt.fxaa_color);
        m_command_list->endMarker();

        m_command_list->close();
        m_device->executeCommandList(m_command_list.Get());
#ifdef DODOE_DEBUG_ENABLED
        if (pick_copied) {
            Size_t row_pitch = 0;
            if (void* data = m_device->mapStagingTexture(m_pick_staging.Get(),
                    cutie::TextureSlice().setSize(1, 1), GfxCpuAccessMode::Read, &row_pitch)) {
                const UInt32 slot = *static_cast<const UInt32*>(data);
                m_device->unmapStagingTexture(m_pick_staging.Get());
                UInt64 picked_uuid = 0;
                if (slot != 0 && static_cast<Size_t>(slot - 1) < m_pick_ids.size()) {
                    picked_uuid = m_pick_ids[slot - 1];
                }
                DebugImGui::SubmitPickResult(picked_uuid);
            }
        }
#endif
        m_device->runGarbageCollection();
    }

} // namespace dodoe
