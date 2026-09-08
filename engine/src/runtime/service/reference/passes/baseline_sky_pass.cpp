// do@Redlive

#include "baseline_sky_pass.h"

#include "runtime/function/render/render_frame/frame_telemetry.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/render_service/binding_layout_cache.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/render_scene/light_scene_info.h"
#include "runtime/function/render/render_pipeline/render_pipeline_pass_utils.h"
#include "runtime/function/render/render_view/render_view.h"
#include "runtime/core/math/math.h"

namespace dodoe {

    namespace {
        constexpr UInt64 kSkyConstantBufferSize = 256;

        struct SkyConstantBuffer {
            Matrix4f inv_view_projection{1.0f};
        };
    }

    Bool BaselineSkyPass::initialize(const BaselinePassContext& context) {
        m_device = context.device;
        m_command_list = context.command_list;
        m_shader_library = context.shader_library;
        m_shared_render_service = context.shared_render_service;
        m_sampler = context.sampler;

        if (!m_shared_render_service || !m_shader_library) {
            return true;
        }
        auto* binding_layout_cache = m_shared_render_service->getBindingLayoutCache();
        DO_ASSERT(binding_layout_cache != nullptr, "BaselineSkyPass: binding layout cache is unavailable");
        m_binding_layout = binding_layout_cache->getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::Pixel)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Pass))
                .addItem(GfxBindingLayoutItem::VolatileConstantBuffer(0))
                .addItem(GfxBindingLayoutItem::Texture_SRV(1))
                .addItem(GfxBindingLayoutItem::Texture_SRV(2))
                .addItem(GfxBindingLayoutItem::Sampler(9)));

        GfxBufferDesc cb_desc;
        cb_desc.setByteSize(static_cast<UInt32>(kSkyConstantBufferSize))
            .setIsConstantBuffer(true)
            .setIsVolatile(true)
            .setMaxVersions(4096)
            .setDebugName("BaselineSkyCB");
        m_sky_cb = m_device->createBuffer(cb_desc);
        return true;
    }

    void BaselineSkyPass::shutdown() {
        m_pipeline = nullptr;
        m_binding_layout = nullptr;
        m_sky_cb = nullptr;
        m_sampler = nullptr;
        m_shader_library = nullptr;
        m_shared_render_service = nullptr;
        m_command_list = nullptr;
        m_device = nullptr;
    }

    void BaselineSkyPass::ensurePipeline(const cutie::FramebufferInfo& framebuffer_info) {
        if (m_pipeline) {
            return;
        }
        if (!m_shader_library) {
            return;
        }
        const auto vertex_shader = m_shader_library->getFullscreenVertexShader();
        const auto pixel_shader = m_shader_library->getSkyboxPixelShader();
        if (!vertex_shader || !pixel_shader) {
            DO_ERROR("BaselineSkyPass: skybox shaders are not loaded");
            return;
        }
        const auto pipeline_desc = rendering_pipeline_utils::BuildFullscreenPipelineDesc(
            vertex_shader, pixel_shader, m_binding_layout, true);
        m_pipeline = m_device->createGraphicsPipeline(pipeline_desc, framebuffer_info);
        DO_INFO("BaselineSkyPass: render pipeline created");
    }

    void BaselineSkyPass::render(RenderView& view, RenderScene& scene, const GfxViewportState& viewport_state,
                                  cutie::IFramebuffer* framebuffer, const GfxTextureHandle& gbuffer_depth) {
        if (!m_pipeline || !gbuffer_depth) {
            return;
        }

        const auto& light_infos = scene.getLightSceneInfos();
        GfxTextureHandle skybox_texture{};
        for (const auto& light_info : light_infos) {
            if (light_info.getLightType() != LightType::Sky || !light_info.isEnabled()) {
                continue;
            }
            const auto& cubemap = light_info.getSkyLightData().cubemap;
            if (cubemap && cubemap->getGpuHandle() && cubemap->getGpuHandle()->isGpuReady()) {
                skybox_texture = cubemap->getGpuHandle();
            }
            break;
        }
        if (!skybox_texture) {
            return;
        }

        GfxBindingSetDesc pass_desc;
        pass_desc.addItem(GfxBindingSetItem::ConstantBuffer(0, m_sky_cb.Get()));
        pass_desc.addItem(GfxBindingSetItem::Texture_SRV(
            1, skybox_texture->getRHIHandle().Get(),
            GfxFormat::UNKNOWN, GfxAllSubresources, GfxTextureDimension::TextureCube));
        pass_desc.addItem(GfxBindingSetItem::Texture_SRV(
            2, gbuffer_depth->getRHIHandle().Get(),
            GfxFormat::UNKNOWN, GfxAllSubresources, GfxTextureDimension::Texture2D));
        pass_desc.addItem(GfxBindingSetItem::Sampler(9, m_sampler.Get()));

        auto binding_set = m_device->createBindingSet(pass_desc, m_binding_layout.Get());

        SkyConstantBuffer cb_data{};
        cb_data.inv_view_projection = Math::Inverse(Math::FlipClipSpaceY(view.getViewProjectionMatrix()));
        m_command_list->writeBuffer(m_sky_cb.Get(), &cb_data, sizeof(cb_data));

        cutie::GraphicsState graphics_state;
        graphics_state.setPipeline(m_pipeline.Get());
        graphics_state.setFramebuffer(framebuffer);
        graphics_state.setViewport(viewport_state);
        graphics_state.addBindingSet(binding_set.Get());
        m_command_list->setGraphicsState(graphics_state);
        RenderFrameCounters::Self().addDrawCall(1); m_command_list->draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
    }

} // namespace dodoe
