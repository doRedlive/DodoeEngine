// do@Redlive

#include "render_skybox_pass.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "render_pass_blackboard_keys.h"

#include "../render_pipeline_pass_utils.h"

#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/pipeline/pipeline_state_cache.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/function/render/shader/global_samplers.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/core/math/math.h"

namespace dodoe {

    struct SkyboxConstantBuffer {
        Matrix4f inv_view_projection{1.0f};
    };

    BEGIN_SHADER_PARAMETER_STRUCT(SkyboxPassShaderParams)
        ShaderParameter<ShaderParamType::ConstantBuffer, 0, GfxBufferHandle> skybox_cb{};
        SHADER_PARAMETER_RAWTEX(0, skybox_texture)
        SHADER_PARAMETER(TextureSRV, 1, depth)
        SHADER_PARAMETER(Sampler,    0, sampler)
    END_SHADER_PARAMETER_STRUCT(func(skybox_cb); func(skybox_texture); func(depth); func(sampler);)

    struct SkyboxPassParameters {
        RenderGraphTextureHandle depth{};
        RenderGraphTextureHandle hdr_color{};
    };

    void SkyboxPass::build(RenderGraphBuilder& graph,
                            const RenderPassBuildContext& context) {
        graph.addPass<SkyboxPassParameters>(
            "SkyboxPass",
            RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
            [&context](RenderGraphPassBuilder& pass_builder, SkyboxPassParameters& parameters) {
                const auto swapchain_extent = context.gfx_context->getSwapchainExtent2d();
                const auto* scene_textures = pass_builder.blackboard().get<SceneTexturesKey, SceneTextures>();
                DO_ASSERT(scene_textures, "SkyboxPass scene textures are missing");

                parameters.depth = pass_builder.read(scene_textures->depth);
                parameters.hdr_color = pass_builder.writeColor(pass_builder.createTransientTexture(
                    rendering_pipeline_utils::MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA16_FLOAT, "RDG MainCameraHdrColor"),
                    "MainCameraHdrColor"));
                pass_builder.blackboard().set<SceneHdrKey>(parameters.hdr_color);
            },
            [this](const SkyboxPassParameters& parameters, const RenderGraphPassContext& ctx, DrawCommandList& command_list) {
                const auto depth_handle = ctx.resolveTexture(parameters.depth);
                const auto hdr = ctx.resolveTexture(parameters.hdr_color);

                auto framebuffer_desc = GfxFramebufferDesc().addColorAttachment(hdr);
                auto fb = command_list.createFramebuffer(framebuffer_desc);

                Bool has_sky = false;
                GfxTextureHandle cubemap_handle{};
                for (const auto& light_info : ctx.getScene()->getLightSceneInfos()) {
                    if (light_info.getLightType() == LightType::Sky && light_info.isEnabled()) {
                        cubemap_handle = light_info.getSkyLightData().cubemap
                            ? light_info.getSkyLightData().cubemap->getGpuHandle()
                            : GfxTextureHandle{};
                        has_sky = true;
                        break;
                    }
                }
                if (!has_sky) {
                    command_list.setTextureState(hdr, GfxAllSubresources, GfxResourceStates::RenderTarget);
                    command_list.commitBarriers();
                    command_list.clearTextureFloat(hdr, GfxAllSubresources, GfxColor(0.0f, 0.0f, 0.0f, 1.0f));
                    return;
                }
                if (!cubemap_handle) {
                    DO_ERROR("SkyboxPass: enabled sky light has no cubemap");
                    command_list.setTextureState(hdr, GfxAllSubresources, GfxResourceStates::RenderTarget);
                    command_list.commitBarriers();
                    command_list.clearTextureFloat(hdr, GfxAllSubresources, GfxColor(0.0f, 0.0f, 0.0f, 1.0f));
                    return;
                }

                SkyboxConstantBuffer cb_data;
                cb_data.inv_view_projection = Math::Inverse(ctx.getView()->getViewProjectionMatrix());
                command_list.writeBuffer(m_skybox_cb, &cb_data, sizeof(cb_data));

                SkyboxPassShaderParams shader_params;
                shader_params.skybox_cb.value = m_skybox_cb;
                shader_params.skybox_texture.value = cubemap_handle;
                shader_params.depth.value = parameters.depth;
                shader_params.sampler.value = GlobalSamplers::screen();

                const auto binding_layout = ShaderBindingReflector<SkyboxPassShaderParams>::getOrCreateLayout();

                auto bs = ShaderBindingReflector<SkyboxPassShaderParams>::createBindingSetDeferred(
                    command_list, binding_layout, shader_params,
                    [&](auto h) { return ctx.resolveTexture(h); },
                    [&](auto h) { return ctx.resolveBuffer(h); }
                );

                if (!bs) {
                    DO_ERROR("SkyboxPass: Failed to create binding set");
                    return;
                }

                GfxFramebufferInfo framebuffer_info(framebuffer_desc);
                auto pipeline = ctx.getPipelineStateCache()->resolveGraphicsPipeline(
                    rendering_pipeline_utils::BuildFullscreenPipelineDesc(
                        ctx.getShaderLibrary()->getFullscreenVertexShader(),
                        ctx.getShaderLibrary()->getSkyboxPixelShader(),
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
                command_list.setGraphicsState(fb, pipeline, bs_arr, rendering_pipeline_utils::BuildViewportState(*ctx.getView(), ctx.getGfxContext()->getSwapchainExtent2d()));
                command_list.draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
            }
        );
    }

} // namespace dodoe
