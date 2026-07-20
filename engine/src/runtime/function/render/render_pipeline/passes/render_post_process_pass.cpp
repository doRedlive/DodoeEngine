// do@Redlive

#include "render_pipeline_passes.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "render_pass_blackboard_keys.h"

#include "../render_pipeline_pass_utils.h"

#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/function/render/shader/global_samplers.h"
#include "runtime/function/render/pipeline/pipeline_state_cache.h"
#include "runtime/function/render/shader/shader_library.h"

namespace dodoe::RenderPipelinePass {

    BEGIN_SHADER_PARAMETER_STRUCT(PostProcessPassShaderParams)
        SHADER_PARAMETER(TextureSRV, 0, input)
        SHADER_PARAMETER(Sampler,    0, sampler)
    END_SHADER_PARAMETER_STRUCT(func(input); func(sampler);)

    struct PostProcessPassParameters {
        RenderGraphTextureHandle input{};
        RenderGraphTextureHandle output{};
    };

    void RenderPostProcessPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context) {
        DO_ASSERT(pass_context.isValid(), "RenderingPipeline pass context is invalid");
        const auto& shader_library = *pass_context.getShaderLibrary();

        graph.addPass<PostProcessPassParameters>(
            "PostProcessPass",
            RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
            [pass_context](RenderGraphPassBuilder& pass_builder, PostProcessPassParameters& parameters) {
                const auto swapchain_extent = pass_context.gfx_context->getSwapchainExtent2d();
                const auto* hdr = pass_builder.blackboard().get<SceneHdrKey, RenderGraphTextureHandle>();
                DO_ASSERT(hdr, "PostProcessPass hdr input is missing");
                parameters.input = pass_builder.read(*hdr);
                parameters.output = pass_builder.write(pass_builder.createTransientTexture(
                    rendering_pipeline_utils::MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA8_UNORM, "RDG PostProcessOutput"),
                    "PostProcessOutput"));
                pass_builder.blackboard().set<SceneColorKey>(parameters.output);
            },
            [pass_context, &shader_library](const PostProcessPassParameters& parameters, const RenderGraphPassContext& context, DrawCommandList& command_list) {
                const auto input_handle = context.resolveTexture(parameters.input);
                const auto output_handle = context.resolveTexture(parameters.output);

                auto framebuffer_desc = GfxFramebufferDesc().addColorAttachment(output_handle);
                auto fb = command_list.createFramebuffer(framebuffer_desc);

                PostProcessPassShaderParams shader_params;
                shader_params.input.value = parameters.input;
                shader_params.sampler.value = GlobalSamplers::screen();

                const auto binding_layout = ShaderBindingReflector<PostProcessPassShaderParams>::getOrCreateLayout();

                auto bs = ShaderBindingReflector<PostProcessPassShaderParams>::createBindingSetDeferred(
                    command_list, binding_layout, shader_params,
                    [&](auto h) { return context.resolveTexture(h); },
                    [&](auto h) { return context.resolveBuffer(h); }
                );

                if (!bs) {
                    DO_ERROR("PostProcessPass: Failed to create binding set");
                    return;
                }

                GfxFramebufferInfo framebuffer_info(framebuffer_desc);
                auto pipeline = pass_context.getPipelineStateCache()->resolveGraphicsPipeline(
                    rendering_pipeline_utils::BuildFullscreenPipelineDesc(
                        shader_library.getFullscreenVertexShader(),
                        shader_library.getToneMappingPixelShader(),
                        binding_layout
                    ),
                    framebuffer_info,
                    command_list
                );

                if (!pipeline) {
                    DO_ERROR("PostProcessPass: Failed to create pipeline");
                    return;
                }

                const auto viewport_state = rendering_pipeline_utils::BuildViewportState(*context.getView(), context.getGfxContext()->getSwapchainExtent2d());

                DynamicArray<GfxBindingSetHandle> bs_arr = {bs};
                command_list.setTextureState(input_handle, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.setTextureState(output_handle, GfxAllSubresources, GfxResourceStates::RenderTarget);
                command_list.commitBarriers();
                command_list.clearTextureFloat(output_handle, GfxAllSubresources, GfxColor(0.0f, 0.0f, 0.0f, 1.0f));
                command_list.setGraphicsState(fb, pipeline, bs_arr, viewport_state);
                command_list.draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
            }
        );
    }

} // namespace dodoe::RenderPipelinePass
