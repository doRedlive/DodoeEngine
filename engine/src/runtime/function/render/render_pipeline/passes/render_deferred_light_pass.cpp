// do@Redlive

#include "render_pipeline_passes.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "render_pass_blackboard_keys.h"

#include "../render_pipeline_pass_utils.h"

#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/framework/pipeline_state_cache.h"
#include "runtime/function/render/framework/shader_library.h"
#include "runtime/function/render/framework/shader_parameter.h"
#include "runtime/function/render/framework/global_samplers.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/core/math/math.h"

namespace dodoe::RenderPipelinePass {

    struct DeferredLightPushConstants {
        Vector4f light_color_intensity{1.0f, 1.0f, 1.0f, 1.0f};
        Vector4f light_position_radius{0.0f, 0.0f, 0.0f, 0.0f};
        Vector4f light_direction_type{0.0f, 0.0f, 0.0f, 0.0f};
        Matrix4f light_view_projection{1.0f};
        Vector4f shadow_params{0.0025f, 0.65f, 0.0f, 0.0f};
        Vector4f camera_position{0.0f, 0.0f, 0.0f, 0.0f};
    };

    BEGIN_SHADER_PARAMETER_STRUCT(DeferredLightPassShaderParams)
        SHADER_PARAMETER(ConstantBuffer, 0, constant_buffer)
        SHADER_PARAMETER(Sampler, 0, sampler)
        SHADER_PARAMETER(TextureSRV, 0, albedo)
        SHADER_PARAMETER(TextureSRV, 1, normal)
        SHADER_PARAMETER(TextureSRV, 2, position)
        SHADER_PARAMETER(TextureSRV, 3, shadow)
        SHADER_PARAMETER(TextureSRV, 4, material)
        SHADER_PARAMETER(TextureSRV, 5, skybox)
    END_SHADER_PARAMETER_STRUCT(func(constant_buffer); func(sampler); func(albedo); func(normal); func(position); func(shadow); func(material); func(skybox);)

    struct DeferredLightPassParameters {
        RenderGraphTextureHandle albedo{};
        RenderGraphTextureHandle normal{};
        RenderGraphTextureHandle position{};
        RenderGraphTextureHandle material{};
        RenderGraphTextureHandle shadow_map{};
        RenderGraphTextureHandle hdr_color{};
        RenderGraphBufferHandle constant_buffer{};
    };

    void RenderDeferredLightPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context) {
        DO_ASSERT(pass_context.isValid(), "RenderingPipeline pass context is invalid");
        const auto& shader_library = *pass_context.getShaderLibrary();

        graph.addPass<DeferredLightPassParameters>(
            "DeferredLightPass",
            RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
            [pass_context](RenderGraphPassBuilder& pass_builder, DeferredLightPassParameters& parameters) {
                const auto* gbuffer = pass_builder.blackboard().get<SceneTexturesKey, SceneTextures>();
                const auto* shadow = pass_builder.blackboard().get<ShadowMapKey, RenderGraphTextureHandle>();
                const auto* hdr = pass_builder.blackboard().get<SceneHdrKey, RenderGraphTextureHandle>();
                DO_ASSERT(gbuffer && shadow && hdr, "DeferredLightPass blackboard resources are missing");
                parameters.albedo = pass_builder.read(gbuffer->albedo);
                parameters.normal = pass_builder.read(gbuffer->normal);
                parameters.position = pass_builder.read(gbuffer->position);
                parameters.material = pass_builder.read(gbuffer->material);
                parameters.shadow_map = pass_builder.read(*shadow);
                parameters.hdr_color = pass_builder.write(*hdr);
                parameters.constant_buffer = pass_builder.importBuffer(pass_context.deferred_light_constant_buffer, "DeferredLightConstantBuffer");
            },
            [pass_context, &shader_library](const DeferredLightPassParameters& parameters, const RenderGraphPassContext& context, DrawCommandList& command_list) {
                const auto device = context.getGfxContext()->getDevice();
                const auto albedo_handle = context.resolveTexture(parameters.albedo);
                const auto normal_handle = context.resolveTexture(parameters.normal);
                const auto position_handle = context.resolveTexture(parameters.position);
                const auto material_handle = context.resolveTexture(parameters.material);
                const auto shadow_handle = context.resolveTexture(parameters.shadow_map);
                const auto hdr = context.resolveTexture(parameters.hdr_color);

                auto framebuffer_desc = GfxFramebufferDesc().addColorAttachment(hdr);
                auto fb = command_list.createFramebuffer( framebuffer_desc);

                DeferredLightPassShaderParams shader_params;
                shader_params.constant_buffer.value = parameters.constant_buffer;
                shader_params.sampler.value = GlobalSamplers::screen();
                shader_params.albedo.value = parameters.albedo;
                shader_params.normal.value = parameters.normal;
                shader_params.position.value = parameters.position;
                shader_params.shadow.value = parameters.shadow_map;
                shader_params.material.value = parameters.material;
                shader_params.skybox.value = parameters.hdr_color;

                const auto binding_layout = ShaderBindingReflector<DeferredLightPassShaderParams>::getOrCreateLayout(device, GfxShaderType::Pixel);

                auto bs = ShaderBindingReflector<DeferredLightPassShaderParams>::createBindingSetDeferred(
                    command_list, binding_layout, shader_params,
                    [&](auto h) { return context.resolveTexture(h); },
                    [&](auto h) { return context.resolveBuffer(h); }
                );

                if (!bs) {
                    DO_ERROR("DeferredLightPass: Failed to create binding set");
                    command_list.setTextureState(hdr, GfxAllSubresources, GfxResourceStates::ShaderResource);
                    command_list.commitBarriers();
                    return;
                }

                GfxFramebufferInfo framebuffer_info(framebuffer_desc);
                auto pipeline = pass_context.getPipelineStateCache()->resolveGraphicsPipeline(
                    rendering_pipeline_utils::BuildFullscreenPipelineDesc(
                        shader_library.getFullscreenVertexShader(),
                        shader_library.getDeferredLightPixelShader(),
                        binding_layout
                    ),
                    framebuffer_info,
                    command_list
                );
                const auto viewport_state = rendering_pipeline_utils::BuildViewportState(*context.getView(), context.getGfxContext()->getSwapchainExtent2d());
                const auto camera_position = rendering_pipeline_utils::ExtractCameraPosition(*context.getView());

                command_list.setTextureState(albedo_handle, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.setTextureState(normal_handle, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.setTextureState(position_handle, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.setTextureState(material_handle, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.setTextureState(shadow_handle, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.setTextureState(hdr, GfxAllSubresources, GfxResourceStates::RenderTarget);
                command_list.commitBarriers();

                const auto& light_infos = context.getScene()->getLightSceneInfos();
                Bool has_enabled_lights = false;
                for (const auto& light_info : light_infos) {
                    if (light_info.isEnabled() && light_info.getLightType() != LightType::Sky) {
                        has_enabled_lights = true;
                        break;
                    }
                }
                if (!has_enabled_lights) {
                    command_list.setTextureState(hdr, GfxAllSubresources, GfxResourceStates::ShaderResource);
                    command_list.commitBarriers();
                    return;
                }

                for (const auto& light_info : light_infos) {
                    if (!light_info.isEnabled()) {
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

                    command_list.setBufferState(context.resolveBuffer(parameters.constant_buffer), GfxResourceStates::CopyDest);
                    command_list.commitBarriers();
                    command_list.writeBuffer(context.resolveBuffer(parameters.constant_buffer), &push, sizeof(push));
                    command_list.setBufferState(context.resolveBuffer(parameters.constant_buffer), GfxResourceStates::ConstantBuffer);
                    command_list.commitBarriers();

                    DynamicArray<GfxBindingSetHandle> bs_arr = {bs};
                    command_list.setGraphicsState(fb, pipeline, bs_arr, viewport_state);
                    command_list.draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
                }

                command_list.setTextureState(hdr, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.commitBarriers();
            }
        );
    }

} // namespace dodoe::RenderPipelinePass
