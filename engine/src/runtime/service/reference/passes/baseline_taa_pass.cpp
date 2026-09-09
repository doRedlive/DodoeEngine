// do@Redlive

#include "baseline_taa_pass.h"

#include "runtime/function/render/render_frame/frame_telemetry.h"
#include "runtime/function/render/render_pipeline/render_pipeline_pass_utils.h"
#include "runtime/function/render/render_service/binding_layout_cache.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/render_view/render_view.h"
#include "runtime/function/render/render_view/taa_view_extension.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/core/math/math.h"

#include <cstring>

namespace dodoe {

    namespace {
        constexpr UInt64 kTaaConstantBufferSize = sizeof(Matrix4f) * 2 + sizeof(Vector4f) * 3;

        struct TaaConstantsData {
            Matrix4f prev_view_projection{1.0f};
            Matrix4f current_view_projection{1.0f};
            Vector4f params{0.0f, 0.0f, 0.0f, 0.9f};
            Vector4f prev_params{0.0f, 0.0f, 0.0f, 0.0f};
            Vector4f tuning{1.5f, 0.01f, 16.0f, 0.2f};
        };

        static_assert(sizeof(TaaConstantsData) == kTaaConstantBufferSize);
        static_assert(kTaaConstantBufferSize <= 256);
    }

    Bool BaselineTaaPass::initialize(const BaselinePassContext& context) {
        m_device = context.device;
        m_command_list = context.command_list;
        m_shader_library = context.shader_library;
        m_shared_render_service = context.shared_render_service;
        m_sampler = context.sampler;

        if (!m_shared_render_service || !m_shader_library) {
            return true;
        }
        auto* binding_layout_cache = m_shared_render_service->getBindingLayoutCache();
        DO_ASSERT(binding_layout_cache != nullptr, "BaselineTaaPass: binding layout cache is unavailable");
        m_binding_layout = binding_layout_cache->getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::Pixel)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Pass))
                .addItem(GfxBindingLayoutItem::VolatileConstantBuffer(0))
                .addItem(GfxBindingLayoutItem::Texture_SRV(1))
                .addItem(GfxBindingLayoutItem::Texture_SRV(2))
                .addItem(GfxBindingLayoutItem::Texture_SRV(3))
                .addItem(GfxBindingLayoutItem::Texture_SRV(4))
                .addItem(GfxBindingLayoutItem::Texture_SRV(5))
                .addItem(GfxBindingLayoutItem::Sampler(9)));

        m_copy_binding_layout = binding_layout_cache->getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::Pixel)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Pass))
                .addItem(GfxBindingLayoutItem::Texture_SRV(1))
                .addItem(GfxBindingLayoutItem::Sampler(9)));

        GfxBufferDesc cb_desc;
        cb_desc.setByteSize(static_cast<UInt32>(kTaaConstantBufferSize))
            .setIsConstantBuffer(true)
            .setIsVolatile(true)
            .setMaxVersions(4096)
            .setDebugName("BaselineTaaCB");
        m_taa_cb = m_device->createBuffer(cb_desc);
        return true;
    }

    void BaselineTaaPass::shutdown() {
        m_pipeline = nullptr;
        m_copy_pipeline = nullptr;
        m_binding_layout = nullptr;
        m_copy_binding_layout = nullptr;
        m_prev_depth_a = nullptr;
        m_prev_depth_b = nullptr;
        m_prev_depth_framebuffer_a = nullptr;
        m_prev_depth_framebuffer_b = nullptr;
        m_prev_depth_extent = Vector2i(0, 0);
        m_taa_cb = nullptr;
        m_sampler = nullptr;
        m_shader_library = nullptr;
        m_shared_render_service = nullptr;
        m_command_list = nullptr;
        m_device = nullptr;
        m_has_prev_frame = false;
    }

    void BaselineTaaPass::ensurePrevDepthTargets(const Vector2i& extent) {
        if (m_prev_depth_a && m_prev_depth_b && m_prev_depth_extent == extent) {
            return;
        }
        if (extent.x <= 0 || extent.y <= 0) {
            return;
        }

        m_prev_depth_a = nullptr;
        m_prev_depth_b = nullptr;
        m_prev_depth_framebuffer_a = nullptr;
        m_prev_depth_framebuffer_b = nullptr;
        m_copy_pipeline = nullptr;
        m_prev_depth_extent = extent;

        GfxTextureDesc depth_desc;
        depth_desc.setDimension(GfxTextureDimension::Texture2D)
            .setFormat(GfxFormat::R32_FLOAT)
            .setWidth(static_cast<UInt32>(extent.x))
            .setHeight(static_cast<UInt32>(extent.y))
            .setIsRenderTarget(true)
            .enableAutomaticStateTracking(GfxResourceStates::RenderTarget)
            .setDebugName("BaselineTaaPrevDepthA");
        m_prev_depth_a = create_ref<GfxTexture>(depth_desc, "BaselineTaaPrevDepthA");
        m_prev_depth_a->initializeGpu(m_device);
        depth_desc.setDebugName("BaselineTaaPrevDepthB");
        m_prev_depth_b = create_ref<GfxTexture>(depth_desc, "BaselineTaaPrevDepthB");
        m_prev_depth_b->initializeGpu(m_device);

        GfxFramebufferDesc fb_desc;
        fb_desc.addColorAttachment(m_prev_depth_a);
        m_prev_depth_framebuffer_a = create_ref<GfxFramebuffer>(fb_desc);
        m_prev_depth_framebuffer_a->initializeGpu(m_device);
        fb_desc = GfxFramebufferDesc();
        fb_desc.addColorAttachment(m_prev_depth_b);
        m_prev_depth_framebuffer_b = create_ref<GfxFramebuffer>(fb_desc);
        m_prev_depth_framebuffer_b->initializeGpu(m_device);
    }

    void BaselineTaaPass::renderPrevDepthCopy(const GfxTextureHandle& depth,
                                              cutie::IFramebuffer* prev_depth_write_framebuffer,
                                              const GfxViewportState& viewport_state) {
        if (!m_shader_library || !depth || !prev_depth_write_framebuffer) {
            return;
        }
        if (!m_copy_pipeline) {
            const auto vertex_shader = m_shader_library->getFullscreenVertexShader();
            const auto pixel_shader = m_shader_library->getTaaDepthCopyPixelShader();
            if (!vertex_shader || !pixel_shader) {
                DO_ERROR("BaselineTaaPass: depth copy shaders are not loaded");
                return;
            }
            const auto pipeline_desc = rendering_pipeline_utils::BuildFullscreenPipelineDesc(
                vertex_shader, pixel_shader, m_copy_binding_layout);
            m_copy_pipeline = m_device->createGraphicsPipeline(
                pipeline_desc, prev_depth_write_framebuffer->getFramebufferInfo());
        }
        if (!m_copy_pipeline) {
            return;
        }

        GfxBindingSetDesc copy_desc;
        copy_desc.addItem(GfxBindingSetItem::Texture_SRV(1, depth->getRHIHandle().Get()));
        copy_desc.addItem(GfxBindingSetItem::Sampler(9, m_sampler.Get()));
        auto copy_binding_set = m_device->createBindingSet(copy_desc, m_copy_binding_layout.Get());
        if (!copy_binding_set) {
            DO_ERROR("BaselineTaaPass: failed to create depth copy binding set");
            return;
        }

        cutie::GraphicsState copy_state;
        copy_state.setPipeline(m_copy_pipeline.Get());
        copy_state.setFramebuffer(prev_depth_write_framebuffer);
        copy_state.setViewport(viewport_state);
        copy_state.addBindingSet(copy_binding_set.Get());
        m_command_list->setGraphicsState(copy_state);
        RenderFrameCounters::Self().addDrawCall(1);
        m_command_list->draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
    }

    void BaselineTaaPass::ensurePipeline(const cutie::FramebufferInfo& framebuffer_info) {
        if (m_pipeline) {
            return;
        }
        if (!m_shader_library) {
            return;
        }
        const auto vertex_shader = m_shader_library->getFullscreenVertexShader();
        const auto pixel_shader = m_shader_library->getTaaPixelShader();
        if (!vertex_shader || !pixel_shader) {
            DO_ERROR("BaselineTaaPass: taa shaders are not loaded");
            return;
        }
        const auto pipeline_desc = rendering_pipeline_utils::BuildFullscreenPipelineDesc(
            vertex_shader, pixel_shader, m_binding_layout);
        m_pipeline = m_device->createGraphicsPipeline(pipeline_desc, framebuffer_info);
        DO_INFO("BaselineTaaPass: render pipeline created");
    }

    void BaselineTaaPass::render(RenderView& view, const Vector2i& extent,
                                 const GfxTextureHandle& scene_color,
                                 const GfxTextureHandle& gbuffer_position,
                                 const GfxTextureHandle& gbuffer_depth,
                                 const GfxTextureHandle& gbuffer_motion,
                                 const GfxTextureHandle& history_read,
                                 const GfxTextureHandle& history_write,
                                 cutie::IFramebuffer* history_write_framebuffer,
                                 const GfxViewportState& viewport_state,
                                 Bool targets_recreated) {
        if (!m_pipeline || !m_taa_cb || !scene_color || !gbuffer_position || !gbuffer_motion || !history_read || !history_write) {
            return;
        }
        (void)extent;

        ensurePrevDepthTargets(extent);
        ++m_frame_counter;
        const Bool depth_flip = (m_frame_counter & 1ull) != 0ull;
        auto* prev_depth_write_framebuffer = depth_flip
            ? m_prev_depth_framebuffer_a->getRHI()
            : m_prev_depth_framebuffer_b->getRHI();
        const GfxTextureHandle prev_depth_read = depth_flip ? m_prev_depth_b : m_prev_depth_a;

        const Bool reset_history = targets_recreated || !m_has_prev_frame;

        if (m_prev_depth_a && m_prev_depth_b && gbuffer_depth) {
            const GfxTextureHandle prev_depth_write = depth_flip ? m_prev_depth_a : m_prev_depth_b;
            m_command_list->setTextureState(prev_depth_write->getRHI(), cutie::AllSubresources, cutie::ResourceStates::RenderTarget);
            m_command_list->setTextureState(prev_depth_read->getRHI(), cutie::AllSubresources, cutie::ResourceStates::ShaderResource);
            m_command_list->setTextureState(gbuffer_depth->getRHI(), cutie::AllSubresources, cutie::ResourceStates::ShaderResource);
            RenderFrameCounters::Self().addBarrier(); m_command_list->commitBarriers();
            renderPrevDepthCopy(gbuffer_depth, prev_depth_write_framebuffer, viewport_state);
            m_command_list->setTextureState(prev_depth_read->getRHI(), cutie::AllSubresources, cutie::ResourceStates::ShaderResource);
            RenderFrameCounters::Self().addBarrier(); m_command_list->commitBarriers();
        }

        GfxBindingSetDesc pass_desc;
        pass_desc.addItem(GfxBindingSetItem::ConstantBuffer(0, m_taa_cb.Get()));
        pass_desc.addItem(GfxBindingSetItem::Texture_SRV(1, scene_color->getRHIHandle().Get()));
        pass_desc.addItem(GfxBindingSetItem::Texture_SRV(2, history_read->getRHIHandle().Get()));
        pass_desc.addItem(GfxBindingSetItem::Texture_SRV(3, gbuffer_position->getRHIHandle().Get()));
        pass_desc.addItem(GfxBindingSetItem::Texture_SRV(4, gbuffer_motion->getRHIHandle().Get()));
        pass_desc.addItem(GfxBindingSetItem::Sampler(9, m_sampler.Get()));
        if (prev_depth_read) {
            pass_desc.addItem(GfxBindingSetItem::Texture_SRV(5, prev_depth_read->getRHIHandle().Get()));
        } else {
            pass_desc.addItem(GfxBindingSetItem::Texture_SRV(5, gbuffer_position->getRHIHandle().Get()));
        }
        auto binding_set = m_device->createBindingSet(pass_desc, m_binding_layout.Get());
        if (!binding_set) {
            DO_ERROR("BaselineTaaPass: failed to create binding set");
            return;
        }

        const auto* taa_extension = view.getExtension<TaaViewExtension>();
        TaaConstantsData cb_data{};
        cb_data.prev_view_projection = Math::FlipClipSpaceY(m_prev_unjittered_view_projection);
        cb_data.current_view_projection = taa_extension
            ? Math::FlipClipSpaceY(taa_extension->unjittered_view_projection)
            : Math::FlipClipSpaceY(view.getViewProjectionMatrix());
        Vector2f current_jitter_uv{0.0f, 0.0f};
        if (taa_extension) {
            current_jitter_uv = Vector2f(
                taa_extension->jitter_ndc.x * 0.5f,
                taa_extension->jitter_ndc.y * 0.5f);
        }
        cb_data.params = Vector4f(
            current_jitter_uv.x,
            current_jitter_uv.y,
            reset_history ? 1.0f : 0.0f,
            reset_history ? 0.0f : 0.9f);
        cb_data.prev_params = Vector4f(m_prev_jitter_uv.x, m_prev_jitter_uv.y, 0.0f, 0.0f);
        m_command_list->writeBuffer(m_taa_cb.Get(), &cb_data, sizeof(cb_data));

        cutie::GraphicsState graphics_state;
        graphics_state.setPipeline(m_pipeline.Get());
        graphics_state.setFramebuffer(history_write_framebuffer);
        graphics_state.setViewport(viewport_state);
        graphics_state.addBindingSet(binding_set.Get());
        m_command_list->setGraphicsState(graphics_state);
        RenderFrameCounters::Self().addDrawCall(1);
        m_command_list->draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));

        if (taa_extension) {
            m_prev_unjittered_view_projection = taa_extension->unjittered_view_projection;
        } else {
            m_prev_unjittered_view_projection = view.getViewProjectionMatrix();
        }
        m_prev_jitter_uv = current_jitter_uv;
        m_has_prev_frame = true;
    }

} // namespace dodoe
