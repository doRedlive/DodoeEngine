// do@Redlive

#include "render_pipeline_passes.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "render_pass_blackboard_keys.h"

#include "runtime/function/render/render_pipeline/render_pipeline_pass_utils.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/framework/pipeline_state_cache.h"
#include "runtime/function/render/framework/shader_library.h"
#include "runtime/function/render/framework/shader_parameter.h"
#include "runtime/function/render/framework/global_samplers.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/core/math/math.h"

namespace dodoe::RenderPipelinePass {

    struct SkyboxPushConstants {
        Matrix4f inv_view_projection{1.0f};
    };

    BEGIN_SHADER_PARAMETER_STRUCT(SkyboxPassShaderParams)
        SHADER_PARAMETER_RAWTEX(0, skybox_texture)
        SHADER_PARAMETER(TextureSRV, 1, depth)
        SHADER_PARAMETER(Sampler,    0, sampler)
        ShaderParameter<ShaderParamType::PushConstants, 0, SkyboxPushConstants> push_constants{};
    END_SHADER_PARAMETER_STRUCT(func(skybox_texture); func(depth); func(sampler); func(push_constants);)

    struct SkyboxPassParameters {
        RenderGraphTextureHandle depth{};
        RenderGraphTextureHandle hdr_color{};
    };

    void RenderSkyboxPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context) {
        DO_ASSERT(pass_context.isValid(), "RenderingPipeline pass context is invalid");
        const auto& shader_library = *pass_context.getShaderLibrary();

        graph.addPass<SkyboxPassParameters>(
            "SkyboxPass",
            RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
            [pass_context](RenderGraphPassBuilder& pass_builder, SkyboxPassParameters& parameters) {
                const auto swapchain_extent = pass_context.gfx_context->getSwapchainExtent2d();
                const auto* scene_textures = pass_builder.blackboard().get<SceneTexturesKey, SceneTextures>();
                DO_ASSERT(scene_textures, "SkyboxPass scene textures are missing");

                parameters.depth = pass_builder.read(scene_textures->depth);
                parameters.hdr_color = pass_builder.write(pass_builder.createTransientTexture(
                    rendering_pipeline_utils::MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA16_FLOAT, "RDG MainCameraHdrColor"),
                    "MainCameraHdrColor"));
                pass_builder.blackboard().set<SceneHdrKey>(parameters.hdr_color);
            },
            [pass_context, &shader_library](const SkyboxPassParameters& parameters, const RenderGraphPassContext& context, DrawCommandList& command_list) {
                const auto device = context.getGfxContext()->getDevice();
                const auto depth_handle = context.resolveTexture(parameters.depth);
                const auto hdr = context.resolveTexture(parameters.hdr_color);

                auto framebuffer_desc = GfxFramebufferDesc().addColorAttachment(hdr);
                auto fb = command_list.createFramebuffer( framebuffer_desc);

                GfxTextureHandle cubemap_handle{};
                for (const auto& light_info : context.getScene()->getLightSceneInfos()) {
                    if (light_info.getLightType() == LightType::Sky && light_info.isEnabled()) {
                        cubemap_handle = light_info.getSkyLightData().cubemap
                            ? light_info.getSkyLightData().cubemap->getGpuHandle()
                            : GfxTextureHandle{};
                        break;
                    }
                }
                if (!cubemap_handle) {
                    cubemap_handle = hdr;
                }

                SkyboxPassShaderParams shader_params;
                shader_params.skybox_texture.value = cubemap_handle;
                shader_params.depth.value = parameters.depth;
                shader_params.sampler.value = GlobalSamplers::screen();
                shader_params.push_constants.value.inv_view_projection = Math::Inverse(context.getView()->getViewProjectionMatrix());

                const auto binding_layout = ShaderBindingReflector<SkyboxPassShaderParams>::getOrCreateLayout(device);

                auto bs = ShaderBindingReflector<SkyboxPassShaderParams>::createBindingSetDeferred(
                    command_list, binding_layout, shader_params,
                    [&](auto h) { return context.resolveTexture(h); },
                    [&](auto h) { return context.resolveBuffer(h); }
                );

                if (!bs) {
                    DO_ERROR("SkyboxPass: Failed to create binding set");
                    return;
                }

                GfxFramebufferInfo framebuffer_info(framebuffer_desc);
                auto pipeline = pass_context.getPipelineStateCache()->resolveGraphicsPipeline(
                    rendering_pipeline_utils::BuildFullscreenPipelineDesc(
                        shader_library.getFullscreenVertexShader(),
                        shader_library.getSkyboxPixelShader(),
                        binding_layout
                    ),
                    framebuffer_info,
                    command_list
                );

                if (!pipeline) {
                    DO_ERROR("SkyboxPass: Failed to create pipeline");
                    return;
                }

                DynamicArray<GfxBindingSetHandle> bs_arr = {bs};
                command_list.setTextureState(hdr, GfxAllSubresources, GfxResourceStates::RenderTarget);
                command_list.setTextureState(depth_handle, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.commitBarriers();
                command_list.clearTextureFloat(hdr, GfxAllSubresources, GfxColor(0.0f, 0.0f, 0.0f, 1.0f));
                command_list.setGraphicsState(fb, pipeline, bs_arr, rendering_pipeline_utils::BuildViewportState(*context.getView(), context.getGfxContext()->getSwapchainExtent2d()));
                command_list.setPushConstants(&shader_params.push_constants.value, sizeof(SkyboxPushConstants));
                command_list.draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
            }
        );
    }

} // namespace dodoe::RenderPipelinePass
