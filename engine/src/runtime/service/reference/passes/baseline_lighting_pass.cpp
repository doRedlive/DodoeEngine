// do@Redlive

#include "baseline_lighting_pass.h"

#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/render_service/binding_layout_cache.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/render_scene/light_scene_info.h"
#include "runtime/function/render/texture/texture_manager.h"
#include "runtime/function/render/render_pipeline/render_pipeline_pass_utils.h"
#include "runtime/core/math/math.h"

namespace dodoe {

    namespace {
        constexpr UInt64 kDeferredLightConstantBufferSize = 256;
    }

    struct DeferredLightPushConstants {
        Vector4f light_color_intensity{1.0f, 1.0f, 1.0f, 1.0f};
        Vector4f light_position_radius{0.0f, 0.0f, 0.0f, 0.0f};
        Vector4f light_direction_type{0.0f, 0.0f, 0.0f, 0.0f};
        Matrix4f light_view_projection{1.0f};
        Vector4f shadow_params{0.0025f, 0.65f, 0.0f, 0.0f};
        Vector4f camera_position{0.0f, 0.0f, 0.0f, 0.0f};
    };

    static_assert(sizeof(DeferredLightPushConstants) <= kDeferredLightConstantBufferSize);

    Bool BaselineLightingPass::initialize(const BaselinePassContext& context) {
        m_device = context.device;
        m_command_list = context.command_list;
        m_shader_library = context.shader_library;
        m_shared_render_service = context.shared_render_service;
        m_sampler = context.sampler;

        if (!m_shared_render_service || !m_shader_library) {
            return true;
        }
        auto* binding_layout_cache = m_shared_render_service->getBindingLayoutCache();
        DO_ASSERT(binding_layout_cache != nullptr, "BaselineLightingPass: binding layout cache is unavailable");
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
                .addItem(GfxBindingLayoutItem::Texture_SRV(6))
                .addItem(GfxBindingLayoutItem::Sampler(9)));

        GfxBufferDesc cb_desc;
        cb_desc.setByteSize(static_cast<UInt32>(kDeferredLightConstantBufferSize))
            .setIsConstantBuffer(true)
            .setIsVolatile(true)
            .setMaxVersions(4096)
            .setDebugName("BaselineLightCB");
        m_light_cb = m_device->createBuffer(cb_desc);
        return true;
    }

    void BaselineLightingPass::shutdown() {
        m_pipeline = nullptr;
        m_binding_layout = nullptr;
        m_light_cb = nullptr;
        m_sampler = nullptr;
        m_shader_library = nullptr;
        m_shared_render_service = nullptr;
        m_command_list = nullptr;
        m_device = nullptr;
    }

    void BaselineLightingPass::ensurePipeline(const cutie::FramebufferInfo& framebuffer_info) {
        if (m_pipeline) {
            return;
        }
        if (!m_shader_library) {
            return;
        }
        const auto vertex_shader = m_shader_library->getFullscreenVertexShader();
        const auto pixel_shader = m_shader_library->getDeferredLightPixelShader();
        if (!vertex_shader || !pixel_shader) {
            DO_ERROR("BaselineLightingPass: deferred light shaders are not loaded");
            return;
        }
        const auto pipeline_desc = rendering_pipeline_utils::BuildFullscreenPipelineDesc(
            vertex_shader, pixel_shader, m_binding_layout, true);
        m_pipeline = m_device->createGraphicsPipeline(pipeline_desc, framebuffer_info);
        DO_INFO("BaselineLightingPass: render pipeline created");
    }

    void BaselineLightingPass::render(RenderView& view, RenderScene& scene, const GfxViewportState& viewport_state,
                                      cutie::IFramebuffer* framebuffer,
                                      const GfxTextureHandle& gbuffer_albedo, const GfxTextureHandle& gbuffer_normal,
                                      const GfxTextureHandle& gbuffer_position, const GfxTextureHandle& gbuffer_material) {
        if (!m_pipeline) {
            return;
        }
        const auto& light_infos = scene.getLightSceneInfos();
        Bool has_enabled_lights = false;
        for (const auto& light_info : light_infos) {
            if (light_info.isEnabled() && light_info.getLightType() != LightType::Sky) {
                has_enabled_lights = true;
                break;
            }
        }

        const auto* texture_manager = scene.getTextureManager();
        GfxTextureHandle shadow_texture{};
        if (texture_manager) {
            if (const auto* fallback = texture_manager->getFallback()) {
                shadow_texture = fallback->getGpuHandle();
            }
        }
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
        if (!skybox_texture && texture_manager) {
            if (const auto* fallback_cubemap = texture_manager->getFallbackCubemap()) {
                skybox_texture = fallback_cubemap->getGpuHandle();
            }
        }

        GfxBindingSetDesc pass_desc;
        pass_desc.addItem(GfxBindingSetItem::ConstantBuffer(0, m_light_cb.Get()));
        pass_desc.addItem(GfxBindingSetItem::Texture_SRV(1, gbuffer_albedo->getRHIHandle().Get()));
        pass_desc.addItem(GfxBindingSetItem::Texture_SRV(2, gbuffer_normal->getRHIHandle().Get()));
        pass_desc.addItem(GfxBindingSetItem::Texture_SRV(3, gbuffer_position->getRHIHandle().Get()));
        pass_desc.addItem(GfxBindingSetItem::Texture_SRV(
            4, shadow_texture ? shadow_texture->getRHIHandle().Get() : nullptr,
            GfxFormat::UNKNOWN, GfxAllSubresources, GfxTextureDimension::Texture2D));
        pass_desc.addItem(GfxBindingSetItem::Texture_SRV(5, gbuffer_material->getRHIHandle().Get()));
        pass_desc.addItem(GfxBindingSetItem::Texture_SRV(
            6, skybox_texture ? skybox_texture->getRHIHandle().Get() : nullptr,
            GfxFormat::UNKNOWN, GfxAllSubresources, GfxTextureDimension::TextureCube));
        pass_desc.addItem(GfxBindingSetItem::Sampler(9, m_sampler.Get()));

        auto binding_set = m_device->createBindingSet(pass_desc, m_binding_layout.Get());

        const auto camera_position = rendering_pipeline_utils::ExtractCameraPosition(view);

        if (!has_enabled_lights) {
            if (!skybox_texture) {
                return;
            }
            DeferredLightPushConstants push{};
            push.camera_position = Vector4f(camera_position, 1.0f);
            m_command_list->writeBuffer(m_light_cb.Get(), &push, sizeof(push));

            cutie::GraphicsState graphics_state;
            graphics_state.setPipeline(m_pipeline.Get());
            graphics_state.setFramebuffer(framebuffer);
            graphics_state.setViewport(viewport_state);
            graphics_state.addBindingSet(binding_set.Get());
            m_command_list->setGraphicsState(graphics_state);
            m_command_list->draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
            return;
        }

        for (const auto& light_info : light_infos) {
            if (!light_info.isEnabled() || light_info.getLightType() == LightType::Sky) {
                continue;
            }

            DeferredLightPushConstants push{};
            push.camera_position = Vector4f(camera_position, 0.0f);

            switch (light_info.getLightType()) {
            case LightType::Directional: {
                const auto& data = light_info.getDirectionalLightData();
                push.light_color_intensity = Vector4f(data.color, data.irradiance);
                push.light_direction_type = Vector4f(Math::Normalize(data.direction), 0.0f);
                push.light_view_projection = rendering_pipeline_utils::BuildDirectionalLightViewProjection(data.direction);
                push.shadow_params = Vector4f(0.005f, 0.2f, 0.005f, 2.0f);
                break;
            }
            case LightType::Point: {
                const auto& data = light_info.getPointLightData();
                push.light_color_intensity = Vector4f(data.color, data.intensity);
                push.light_position_radius = Vector4f(Vector3f(light_info.getWorldTransform()[3]), data.radius);
                push.light_direction_type = Vector4f(0.0f, 0.0f, 0.0f, data.range);
                break;
            }
            case LightType::Spot: {
                const auto& data = light_info.getSpotLightData();
                push.light_color_intensity = Vector4f(data.color, data.intensity);
                push.light_position_radius = Vector4f(Vector3f(light_info.getWorldTransform()[3]), data.radius);
                const Vector3f forward = Math::Normalize(Vector3f(light_info.getWorldTransform()[2]));
                push.light_direction_type = Vector4f(forward.x, forward.y, forward.z, data.range);
                push.shadow_params = Vector4f(data.inner_angle, data.outer_angle, 0.0f, 0.0f);
                break;
            }
            case LightType::Sky:
            default:
                continue;
            }

            m_command_list->writeBuffer(m_light_cb.Get(), &push, sizeof(push));

            cutie::GraphicsState graphics_state;
            graphics_state.setPipeline(m_pipeline.Get());
            graphics_state.setFramebuffer(framebuffer);
            graphics_state.setViewport(viewport_state);
            graphics_state.addBindingSet(binding_set.Get());
            m_command_list->setGraphicsState(graphics_state);
            m_command_list->draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
        }
    }

} // namespace dodoe
