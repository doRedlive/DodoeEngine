// do@Redlive

#include "render_post_process_2d_pass.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "render_pass_blackboard_keys.h"

#include "../render_pipeline_pass_utils.h"

#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/function/render/shader/global_samplers.h"
#include "runtime/function/render/pipeline/pipeline_state_cache.h"
#include "runtime/function/render/shader/shader_library.h"

namespace dodoe {

    BEGIN_SHADER_PARAMETER_STRUCT(PostProcess2DPassShaderParams)
        SHADER_PARAMETER(TextureSRV, 0, input)
        SHADER_PARAMETER(Sampler,    0, sampler)
    END_SHADER_PARAMETER_STRUCT(func(input); func(sampler);)

    struct PostProcess2DPassParameters {
        RenderGraphTextureHandle input{};
        RenderGraphTextureHandle output{};
    };

    void PostProcess2DPass::build(RenderGraphBuilder& graph,
                                   const RenderPassBuildContext& context) {
        const auto* shader_library = context.shared_render_service->getShaderLibrary();
        DO_ASSERT(shader_library != nullptr, "PostProcess2DPass shader library is null");

        graph.addPass<PostProcess2DPassParameters>(
            "PostProcess2DPass",
            RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
            [&context](RenderGraphPassBuilder& pass_builder, PostProcess2DPassParameters& parameters) {
                const auto swapchain_extent = context.gfx_context->getSwapchainExtent2d();
                const auto* scene_color = pass_builder.blackboard().get<SceneColorKey, RenderGraphTextureHandle>();
                DO_ASSERT(scene_color, "PostProcess2DPass scene color is missing");
                parameters.input = pass_builder.read(*scene_color);
                parameters.output = pass_builder.writeColor(pass_builder.createTransientTexture(
                    rendering_pipeline_utils::MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA8_UNORM, "RDG PostProcess2DOutput"),
                    "PostProcess2DOutput"));
                pass_builder.blackboard().set<SceneColorKey>(parameters.output);
            },
            [shader_library](const PostProcess2DPassParameters& parameters, const RenderGraphPassContext& ctx, DrawCommandList& command_list) {
                const auto input_handle = ctx.resolveTexture(parameters.input);
                const auto output_handle = ctx.resolveTexture(parameters.output);

                auto framebuffer_desc = GfxFramebufferDesc().addColorAttachment(output_handle);
                auto fb = command_list.createFramebuffer(framebuffer_desc);

                PostProcess2DPassShaderParams shader_params;
                shader_params.input.value = parameters.input;
                shader_params.sampler.value = GlobalSamplers::screen();

                const auto binding_layout = ShaderBindingReflector<PostProcess2DPassShaderParams>::getOrCreateLayout();

                auto bs = ShaderBindingReflector<PostProcess2DPassShaderParams>::createBindingSetDeferred(
                    command_list, binding_layout, shader_params,
                    [&](auto h) { return ctx.resolveTexture(h); },
                    [&](auto h) { return ctx.resolveBuffer(h); }
                );

                if (!bs) {
                    DO_ERROR("PostProcess2DPass: Failed to create binding set");
                    return;
                }

                GfxFramebufferInfo framebuffer_info(framebuffer_desc);
                auto pipeline = ctx.getPipelineStateCache()->resolveGraphicsPipeline(
                    rendering_pipeline_utils::BuildFullscreenPipelineDesc(
                        shader_library->getFullscreenVertexShader(),
                        shader_library->getFxaaPixelShader(),
                        binding_layout
                    ),
                    framebuffer_info,
                    command_list
                );

                if (!pipeline) {
                    DO_ERROR("PostProcess2DPass: Failed to create pipeline");
                    return;
                }

                const auto viewport_state = rendering_pipeline_utils::BuildViewportState(*ctx.getView(), ctx.getGfxContext()->getSwapchainExtent2d());

                DynamicArray<GfxBindingSetHandle> bs_arr = {bs};
                command_list.setTextureState(input_handle, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.setTextureState(output_handle, GfxAllSubresources, GfxResourceStates::RenderTarget);
                command_list.commitBarriers();
                command_list.clearTextureFloat(output_handle, GfxAllSubresources, GfxColor(0.0f, 0.0f, 0.0f, 1.0f));
                command_list.setGraphicsState(fb, pipeline, bs_arr, viewport_state);
                command_list.draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
                command_list.setTextureState(output_handle, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.commitBarriers();
            }
        );
    }

} // namespace dodoe
