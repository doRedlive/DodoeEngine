// do@Redlive

#include "render_skybox_pass.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "render_pass_blackboard_keys.h"

#include "../render_pipeline_pass_utils.h"

#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/pipeline_state/pipeline_state_cache.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/function/render/shader/global_samplers.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_pipeline/render_graph_import_keys.h"
#include "runtime/function/render/render_pipeline/render_graph_import_registry.h"
#include "runtime/core/math/math.h"

namespace dodoe {

    struct SkyboxConstantBuffer {
        Matrix4f inv_view_projection{1.0f};
    };

    BEGIN_SHADER_PARAMETER_STRUCT(SkyboxPassShaderParams, Pass)
        SHADER_PARAMETER_RAWCB(0, skybox_cb)
        SHADER_PARAMETER_RAWTEX(1, skybox_texture)
        SHADER_PARAMETER(TextureSRV, 2, depth)
        SHADER_PARAMETER(Sampler, 9, sampler)
    END_SHADER_PARAMETER_STRUCT(func(skybox_cb); func(skybox_texture); func(depth); func(sampler);)

    struct SkyboxPassParameters {
        RenderGraphTextureHandle depth{};
        RenderGraphTextureHandle hdr_color{};
        RenderGraphTextureHandle skybox_texture{};
        RenderGraphBufferHandle skybox_cb{};
        Bool has_sky_light{false};
    };

    void SkyboxPass::build(RenderGraphBuilder& graph,
                            const RenderPassBuildContext& context) {
        graph.addPass<SkyboxPassParameters>(
            "SkyboxPass",
            RenderGraphPassFlags::Raster,
            [&context](RenderGraphPassBuilder& pass_builder, SkyboxPassParameters& parameters) {
                const auto swapchain_extent = context.gfx_context->getSwapchainExtent2D();
                const auto* scene_textures = pass_builder.blackboard().get<SceneTexturesKey>();
                DO_ASSERT(scene_textures, "SkyboxPass scene textures are missing");

                parameters.depth = pass_builder.read(scene_textures->depth);
                const auto* existing_hdr = pass_builder.blackboard().get<SceneHdrKey>();
                RenderGraphAttachmentInfo hdr_attachment{};
                if (existing_hdr && existing_hdr->isValid()) {
                    hdr_attachment.load_op = LoadOp::Load;
                    parameters.hdr_color = pass_builder.writeColor(*existing_hdr, hdr_attachment);
                } else {
                    hdr_attachment.load_op = LoadOp::Clear;
                    hdr_attachment.clear_color = GfxColor(0.0f, 0.0f, 0.0f, 1.0f);
                    parameters.hdr_color = pass_builder.writeColor(pass_builder.createTransientTexture(
                        rendering_pipeline_utils::MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA16_FLOAT, "RDG SceneHdrColor"),
                        "SceneHdrColor"), hdr_attachment);
                    pass_builder.blackboard().set<SceneHdrKey>(parameters.hdr_color);
                }

                DO_ASSERT(context.graph_imports != nullptr, "SkyboxPass graph imports are null");
                parameters.skybox_cb = pass_builder.write(pass_builder.importBuffer(
                    context.graph_imports->require<SkyboxConstantBufferKey>(), "SkyboxConstantBuffer"));

                if (!context.scene) {
                    return;
                }
                for (const auto& light_info : context.scene->getLightSceneInfos()) {
                    if (light_info.getLightType() != LightType::Sky || !light_info.isEnabled()) {
                        continue;
                    }
                    parameters.has_sky_light = true;
                    const auto cubemap = light_info.getSkyLightData().cubemap;
                    if (cubemap && cubemap->getGpuHandle()) {
                        parameters.skybox_texture = pass_builder.read(pass_builder.importTexture(
                            cubemap->getGpuHandle(), "SkyboxCubemap"));
                    }
                    break;
                }
            },
            [this](const SkyboxPassParameters& parameters, const RenderGraphPassContext& ctx, DrawCommandList& command_list) {
                if (!parameters.has_sky_light) {
                    return;
                }
                if (!parameters.skybox_texture.isValid()) {
                    DO_ERROR("SkyboxPass: enabled sky light has no cubemap");
                    return;
                }

                const auto skybox_cb = ctx.resolveBuffer(parameters.skybox_cb);
                const auto cubemap_handle = ctx.resolveTexture(parameters.skybox_texture);

                SkyboxConstantBuffer cb_data;
                cb_data.inv_view_projection = Math::Inverse(ctx.getView()->getViewProjectionMatrix());
                command_list.setBufferState(skybox_cb, GfxResourceStates::CopyDest);
                command_list.commitBarriers();
                command_list.writeBuffer(skybox_cb, &cb_data, sizeof(cb_data));
                command_list.setBufferState(skybox_cb, GfxResourceStates::ConstantBuffer);

                SkyboxPassShaderParams shader_params;
                shader_params.skybox_cb.value = skybox_cb;
                shader_params.skybox_texture.value = cubemap_handle;
                shader_params.depth.value = parameters.depth;
                shader_params.sampler.value = GlobalSamplers::screen();

                const auto binding_layouts = ShaderBindingReflector<SkyboxPassShaderParams>::getOrCreateLayouts();

                auto binding_sets = ShaderBindingReflector<SkyboxPassShaderParams>::createBindingSets(
                    command_list, binding_layouts, shader_params,
                    [&](auto h) { return ctx.resolveTexture(h); },
                    [&](auto h) { return ctx.resolveBuffer(h); }
                );

                if (binding_sets.empty()) {
                    DO_ERROR("SkyboxPass: Failed to create binding set");
                    return;
                }

                auto pipeline = ctx.getPipelineStateCache()->resolveGraphicsPipeline(
                    rendering_pipeline_utils::BuildFullscreenPipelineDesc(
                        ctx.getShaderLibrary()->getFullscreenVertexShader(),
                        ctx.getShaderLibrary()->getSkyboxPixelShader(),
                        binding_layouts
                    ),
                    ctx.getRenderTargetSignature(),
                    command_list
                );

                if (!pipeline) {
                    DO_ERROR("SkyboxPass: Failed to create pipeline");
                    return;
                }

                command_list.setGraphicsState(ctx.getFramebuffer(), pipeline, binding_sets, rendering_pipeline_utils::BuildViewportState(*ctx.getView(), ctx.getGfxContext()->getSwapchainExtent2D()));
                command_list.draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
            }
        );
    }

} // namespace dodoe
