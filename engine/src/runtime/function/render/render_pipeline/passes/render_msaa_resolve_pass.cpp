// do@Redlive

#include "render_msaa_resolve_pass.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "runtime/function/render/render_pipeline/passes/render_pass_blackboard_keys.h"

#include "runtime/function/render/render_pipeline/render_pipeline_pass_utils.h"
#include "runtime/function/render/render_settings.h"

#include "runtime/function/render/render_graph/render_graph_builder.h"

namespace dodoe {

    struct MsaaResolvePassParameters {
        RenderGraphTextureHandle source{};
        RenderGraphTextureHandle destination{};
    };

    void MsaaResolvePass::build(RenderGraphBuilder& graph,
                                const RenderPassBuildContext& context) {
        graph.addPass<MsaaResolvePassParameters>(
            "MsaaResolvePass",
            RenderGraphPassFlags::Copy,
            [&context](RenderGraphPassBuilder& pass_builder, MsaaResolvePassParameters& parameters) {
                const auto swapchain_extent = context.gfx_context->getSwapchainExtent2D();
                const auto* hdr = pass_builder.blackboard().get<SceneHdrKey>();
                DO_ASSERT(hdr, "MsaaResolvePass hdr input is missing");
                parameters.source = pass_builder.readTexture(*hdr, RenderGraphPipelineStage::Copy);
                parameters.destination = pass_builder.writeTexture(pass_builder.createTransientTexture(
                    rendering_pipeline_utils::MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA16_FLOAT, "RDG MsaaResolvedHdr"),
                    "MsaaResolvedHdr"), RenderGraphPipelineStage::Copy);
                pass_builder.blackboard().set<SceneHdrKey>(parameters.destination);
            },
            [](const MsaaResolvePassParameters& parameters,
               const RenderGraphPassContext& ctx,
               DrawCommandList& command_list) {
                const auto source = ctx.resolveTexture(parameters.source);
                const auto destination = ctx.resolveTexture(parameters.destination);
                if (!source || !destination) {
                    DO_ERROR("MsaaResolvePass: failed to resolve graph textures");
                    return;
                }

                command_list.setTextureState(source, GfxAllSubresources, GfxResourceStates::ResolveSource);
                command_list.setTextureState(destination, GfxAllSubresources, GfxResourceStates::ResolveDest);
                command_list.commitBarriers();
                command_list.resolveTexture(destination, source);
                command_list.setTextureState(destination, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.commitBarriers();
            });
    }

} // namespace dodoe
