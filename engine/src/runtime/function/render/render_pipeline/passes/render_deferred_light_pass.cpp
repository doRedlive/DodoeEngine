// do@Redlive

#include "render_deferred_light_pass.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "render_pass_blackboard_keys.h"

#include "../render_pipeline_pass_utils.h"

#include "runtime/function/render/render_frame/frame_staging_allocator.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/render_view/mesh_view_extension.h"
#include "runtime/function/render/render_service/binding_layout_cache.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/pipeline_state/pipeline_state_cache.h"
#include "runtime/function/render/shader/global_samplers.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/core/math/math.h"

#include <cstring>

namespace dodoe {

    namespace {
        constexpr UInt64 kDeferredLightConstantBufferSize = 320;
    }

    struct DeferredLightPushConstants {
        Vector4f light_color_intensity{1.0f, 1.0f, 1.0f, 1.0f};
        Vector4f light_position_radius{0.0f, 0.0f, 0.0f, 0.0f};
        Vector4f light_direction_type{0.0f, 0.0f, 0.0f, 0.0f};
        Matrix4f light_view_projection{1.0f};
        Vector4f shadow_params{0.0025f, 0.65f, 0.0f, 0.0f};
        Vector4f camera_position{0.0f, 0.0f, 0.0f, 0.0f};
        Vector4f irradiance_sh[9]{};
        Vector4f ibl_params{0.0f, 0.35f, 0.0f, 0.0f};
        Vector4f emissive_params{0.0f, 0.0f, 0.0f, 0.0f};
    };

    static_assert(sizeof(DeferredLightPushConstants) <= kDeferredLightConstantBufferSize);

    struct DeferredLightPassParameters {
        RenderGraphTextureHandle albedo{};
        RenderGraphTextureHandle normal{};
        RenderGraphTextureHandle position{};
        RenderGraphTextureHandle material{};
        RenderGraphTextureHandle emissive{};
        RenderGraphTextureHandle shadow_map{};
        RenderGraphTextureHandle hdr_color{};
        RenderGraphTextureHandle skybox_texture{};
    };

    void DeferredLightPass::build(RenderGraphBuilder& graph,
                                  const RenderPassBuildContext& context) {
        const auto* shader_library = context.shared_render_service->getShaderLibrary();
        auto* binding_layout_cache = context.shared_render_service->getBindingLayoutCache();
        DO_ASSERT(shader_library != nullptr, "DeferredLightPass shader library is null");
        DO_ASSERT(binding_layout_cache != nullptr, "DeferredLightPass binding layout cache is null");

        const auto binding_layout = binding_layout_cache->getOrCreate(
            GfxBindingLayoutDesc()
                .setVisibility(GfxShaderType::Pixel)
                .setRegisterSpaceIsDescriptorSet(true)
                .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Pass))
                .addItem(GfxBindingLayoutItem::ConstantBuffer(0))
                .addItem(GfxBindingLayoutItem::Texture_SRV(1))
                .addItem(GfxBindingLayoutItem::Texture_SRV(2))
                .addItem(GfxBindingLayoutItem::Texture_SRV(3))
                .addItem(GfxBindingLayoutItem::Texture_SRV(4))
                .addItem(GfxBindingLayoutItem::Texture_SRV(5))
                .addItem(GfxBindingLayoutItem::Texture_SRV(6))
                .addItem(GfxBindingLayoutItem::Texture_SRV(7))
                .addItem(GfxBindingLayoutItem::Texture_SRV(10))
                .addItem(GfxBindingLayoutItem::Sampler(9)));

        graph.addPass<DeferredLightPassParameters>(
            "DeferredLightPass",
            RenderGraphPassFlags::Raster,
            [&context](RenderGraphPassBuilder& pass_builder, DeferredLightPassParameters& parameters) {
                const auto* gbuffer = pass_builder.blackboard().get<SceneTexturesKey>();
                const auto* shadow = pass_builder.blackboard().get<ShadowMapKey>();
                const auto* hdr = pass_builder.blackboard().get<SceneHdrKey>();
                DO_ASSERT(gbuffer && shadow && hdr, "DeferredLightPass blackboard resources are missing");
                parameters.albedo = pass_builder.read(gbuffer->albedo);
                parameters.normal = pass_builder.read(gbuffer->normal);
                parameters.position = pass_builder.read(gbuffer->position);
                parameters.material = pass_builder.read(gbuffer->material);
                parameters.emissive = pass_builder.read(gbuffer->emissive);
                parameters.shadow_map = pass_builder.read(*shadow);
                RenderGraphAttachmentInfo hdr_attachment{};
                hdr_attachment.load_op = LoadOp::Load;
                parameters.hdr_color = pass_builder.writeColor(*hdr, hdr_attachment);

                if (!context.scene) {
                    return;
                }
                for (const auto& light_info : context.scene->getLightSceneInfos()) {
                    if (light_info.getLightType() != LightType::Sky || !light_info.isEnabled()) {
                        continue;
                    }
                    const auto cubemap = light_info.getSkyLightData().cubemap;
                    if (cubemap && cubemap->getGpuHandle()) {
                        parameters.skybox_texture = pass_builder.read(pass_builder.importTexture(
                            cubemap->getGpuHandle(), "DeferredLightSkyboxCubemap"));
                    }
                    break;
                }
            },
            [shader_library, binding_layout](const DeferredLightPassParameters& parameters,
                                             const RenderGraphPassContext& ctx,
                                             DrawCommandList& command_list) {
                const auto& light_infos = ctx.getScene()->getLightSceneInfos();

                auto* staging = ctx.getFrameStagingAllocator();
                if (!staging) {
                    DO_ERROR("DeferredLightPass: frame staging allocator is null");
                    return;
                }

                const auto albedo_handle = ctx.resolveTexture(parameters.albedo);
                const auto normal_handle = ctx.resolveTexture(parameters.normal);
                const auto position_handle = ctx.resolveTexture(parameters.position);
                const auto material_handle = ctx.resolveTexture(parameters.material);
                const auto emissive_handle = ctx.resolveTexture(parameters.emissive);
                const auto shadow_handle = ctx.resolveTexture(parameters.shadow_map);
                GfxTextureHandle skybox_texture{};
                if (parameters.skybox_texture.isValid()) {
                    skybox_texture = ctx.resolveTexture(parameters.skybox_texture);
                }
                Float sky_intensity = 0.0f;
                UInt32 sky_max_mip = 1;
                Vector4f sky_irradiance_sh[9]{};
                Bool has_enabled_sky = false;
                for (const auto& light_info : light_infos) {
                    if (light_info.getLightType() != LightType::Sky || !light_info.isEnabled()) {
                        continue;
                    }
                    has_enabled_sky = true;
                    const auto& sky_data = light_info.getSkyLightData();
                    const auto& cubemap = sky_data.cubemap;
                    if (cubemap && cubemap->getGpuHandle()) {
                        if (!skybox_texture) {
                            skybox_texture = cubemap->getGpuHandle();
                        }
                        if (const Vector4f* sh = cubemap->getIrradianceSH()) {
                            for (UInt32 b = 0; b < 9u; ++b) {
                                sky_irradiance_sh[b] = sh[b];
                            }
                        }
                        sky_intensity = sky_data.intensity;
                        UInt32 face_size = static_cast<UInt32>(cubemap->getFaceSize());
                        UInt32 mip_count = 1;
                        while ((face_size >> mip_count) != 0) {
                            ++mip_count;
                        }
                        sky_max_mip = mip_count - 1;
                    }
                    break;
                }
                if (!skybox_texture) {
                    if (const auto* fallback_cubemap = ctx.getTextureManager()->getFallbackCubemap()) {
                        skybox_texture = fallback_cubemap->getGpuHandle();
                    }
                }
                const auto* brdf_lut = ctx.getTextureManager()->getBrdfLut();
                const GfxTextureHandle brdf_lut_handle = brdf_lut ? brdf_lut->getGpuHandle() : GfxTextureHandle{};

                const auto pipeline = ctx.getPipelineStateCache()->resolveGraphicsPipeline(
                    rendering_pipeline_utils::BuildFullscreenPipelineDesc(
                        shader_library->getFullscreenVertexShader(),
                        shader_library->getDeferredLightPixelShader(),
                        binding_layout,
                        true),
                    ctx.getRenderTargetSignature(),
                    command_list);
                if (!pipeline) {
                    DO_ERROR("DeferredLightPass: failed to create pipeline");
                    return;
                }

                const auto viewport_state = rendering_pipeline_utils::BuildViewportState(
                    *ctx.getView(), ctx.getGfxContext()->getSwapchainExtent2D());
                const auto camera_position = rendering_pipeline_utils::ExtractCameraPosition(*ctx.getView());

                auto draw_fullscreen_light = [&](const DeferredLightPushConstants& push) {
                    const auto allocation = staging->allocate(kDeferredLightConstantBufferSize);
                    if (!allocation.buffer || !allocation.mapped_data) {
                        DO_ERROR("DeferredLightPass: unable to allocate light constant buffer");
                        return;
                    }
                    std::memset(allocation.mapped_data, 0, static_cast<Size_t>(allocation.size));
                    std::memcpy(allocation.mapped_data, &push, sizeof(push));

                    const auto binding_set = command_list.createBindingSet(
                        GfxBindingSetDesc()
                            .addItem(GfxBindingSetItem::ConstantBuffer(
                                0, allocation.buffer->getRHIHandle().Get(),
                                GfxBufferRange(allocation.offset, allocation.size)))
                            .addItem(GfxBindingSetItem::Texture_SRV(1, albedo_handle->getRHIHandle().Get()))
                            .addItem(GfxBindingSetItem::Texture_SRV(2, normal_handle->getRHIHandle().Get()))
                            .addItem(GfxBindingSetItem::Texture_SRV(3, position_handle->getRHIHandle().Get()))
                            .addItem(GfxBindingSetItem::Texture_SRV(4, shadow_handle->getRHIHandle().Get()))
                            .addItem(GfxBindingSetItem::Texture_SRV(5, material_handle->getRHIHandle().Get()))
                            .addItem(GfxBindingSetItem::Texture_SRV(
                                6, skybox_texture ? skybox_texture->getRHIHandle().Get() : nullptr,
                                GfxFormat::UNKNOWN, GfxAllSubresources, GfxTextureDimension::TextureCube))
                            .addItem(GfxBindingSetItem::Texture_SRV(
                                7, brdf_lut_handle ? brdf_lut_handle->getRHIHandle().Get() : nullptr))
                            .addItem(GfxBindingSetItem::Texture_SRV(
                                10, emissive_handle ? emissive_handle->getRHIHandle().Get() : nullptr))
                            .addItem(GfxBindingSetItem::Sampler(9, GlobalSamplers::Screen().Get())),
                        binding_layout);
                    if (!binding_set) {
                        DO_ERROR("DeferredLightPass: failed to create binding set");
                        return;
                    }

                    DynamicArray<GfxBindingSetHandle> binding_sets = {binding_set};
                    command_list.setGraphicsState(ctx.getFramebuffer(), pipeline, binding_sets, viewport_state);
                    command_list.draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
                };

                {
                    DeferredLightPushConstants push{};
                    push.camera_position = Vector4f(camera_position, has_enabled_sky ? 1.0f : 0.0f);
                    if (has_enabled_sky) {
                        for (UInt32 b = 0; b < 9u; ++b) {
                            push.irradiance_sh[b] = sky_irradiance_sh[b];
                        }
                        push.ibl_params = Vector4f(sky_intensity, 0.35f, static_cast<Float>(sky_max_mip), 0.0f);
                    }
                    push.light_color_intensity = Vector4f(0.0f, 0.0f, 0.0f, 0.0f);
                    push.light_direction_type = Vector4f(0.0f, -1.0f, 0.0f, 0.0f);
                    push.emissive_params = Vector4f(1.0f, 0.0f, 0.0f, 0.0f);
                    draw_fullscreen_light(push);
                }

                for (const auto& light_info : light_infos) {
                    if (!light_info.isEnabled() || light_info.getLightType() == LightType::Sky) {
                        continue;
                    }

                    DeferredLightPushConstants push{};
                    push.camera_position = Vector4f(camera_position, 0.0f);
                    push.ibl_params = Vector4f(0.0f, 0.35f, static_cast<Float>(sky_max_mip), 0.0f);

                    switch (light_info.getLightType()) {
                    case LightType::Directional: {
                        const auto& data = light_info.getDirectionalLightData();
                        push.light_color_intensity = Vector4f(data.color, data.irradiance);
                        push.light_direction_type = Vector4f(Math::Normalize(data.direction), 0.0f);
                        const auto* mesh_ext = ctx.getView()->getExtension<MeshViewExtension>();
                        push.light_view_projection = mesh_ext
                            ? mesh_ext->directional_shadow_view_projection
                            : rendering_pipeline_utils::BuildDirectionalLightViewProjection(data.direction);
                        if (mesh_ext) {
                            push.shadow_params = mesh_ext->directional_shadow_params;
                        }
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

                    draw_fullscreen_light(push);
                }
            });
    }

} // namespace dodoe
