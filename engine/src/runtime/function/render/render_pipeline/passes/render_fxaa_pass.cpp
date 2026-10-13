// do@Redlive

#include "render_fxaa_pass.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "render_pass_blackboard_keys.h"

#include "../render_pipeline_pass_utils.h"
#include "runtime/function/render/render_settings.h"
#include "runtime/function/render/render_service/shared_render_service.h"

#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/function/render/shader/global_samplers.h"
#include "runtime/function/render/pipeline_state/pipeline_state_cache.h"
#include "runtime/function/render/shader/shader_library.h"

namespace dodoe {

    BEGIN_SHADER_PARAMETER_STRUCT(FxaaPassShaderParams, Pass)
        SHADER_PARAMETER(TextureSRV, 1, input)
        SHADER_PARAMETER(Sampler, 9, sampler)
    END_SHADER_PARAMETER_STRUCT(func(input); func(sampler);)

    struct FxaaPassParameters {
        RenderGraphTextureHandle input{};
        RenderGraphTextureHandle output{};
    };

    void FxaaPass::build(RenderGraphBuilder& graph,
                         const RenderPassBuildContext& context) {
        if (!RenderSettings::IsFxaaEnabled()) {
            return;
        }

        const auto* shader_library = context.shared_render_service->getShaderLibrary();
        DO_ASSERT(shader_library != nullptr, "FxaaPass shader library is null");

        graph.addPass<FxaaPassParameters>(
            "FxaaPass",
            RenderGraphPassFlags::Raster,
            [&context](RenderGraphPassBuilder& pass_builder, FxaaPassParameters& parameters) {
                const auto swapchain_extent = context.gfx_context->getSwapchainExtent2D();
                const auto* scene_color = pass_builder.blackboard().get<SceneColorKey>();
                DO_ASSERT(scene_color, "FxaaPass scene color is missing");
                parameters.input = pass_builder.read(*scene_color);
                RenderGraphAttachmentInfo output_attachment{};
                output_attachment.load_op = LoadOp::Clear;
                output_attachment.clear_color = GfxColor(0.0f, 0.0f, 0.0f, 1.0f);
                parameters.output = pass_builder.writeColor(pass_builder.createTransientTexture(
                    rendering_pipeline_utils::MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA8_UNORM, "RDG FxaaOutput"),
                    "FxaaOutput"), output_attachment);
                pass_builder.blackboard().set<SceneColorKey>(parameters.output);
            },
            [shader_library](const FxaaPassParameters& parameters, const RenderGraphPassContext& ctx, DrawCommandList& command_list) {
                FxaaPassShaderParams shader_params;
                shader_params.input.value = parameters.input;
                shader_params.sampler.value = GlobalSamplers::Screen();

                const auto binding_layouts = ShaderBindingReflector<FxaaPassShaderParams>::getOrCreateLayouts();

                auto binding_sets = ShaderBindingReflector<FxaaPassShaderParams>::createBindingSets(
                    *ctx.getSharedRenderService()->getBindingSetCache(),
                    binding_layouts, shader_params,
                    [&](auto h) { return ctx.resolveTexture(h); },
                    [&](auto h) { return ctx.resolveBuffer(h); });

                if (binding_sets.empty()) {
                    DO_ERROR("FxaaPass: Failed to create binding set");
                    return;
                }

                auto pipeline = ctx.getPipelineStateCache()->resolveGraphicsPipeline(
                    rendering_pipeline_utils::BuildFullscreenPipelineDesc(
                        shader_library->getFullscreenVertexShader(),
                        shader_library->getFxaaPixelShader(),
                        binding_layouts
                    ),
                    ctx.getRenderTargetSignature(),
                    command_list
                );

                if (!pipeline) {
                    DO_ERROR("FxaaPass: Failed to create pipeline");
                    return;
                }

                const auto viewport_state = rendering_pipeline_utils::BuildViewportState(*ctx.getView(), ctx.getGfxContext()->getSwapchainExtent2D());

                command_list.setGraphicsState(ctx.getFramebuffer(), pipeline, binding_sets, viewport_state);
                command_list.draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
            }
        );
    }

} // namespace dodoe
