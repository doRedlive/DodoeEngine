// do@Redlive

#include "render_builtin_features.h"

#include "runtime/function/render/render_pipeline/passes/render_pipeline_passes.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_view/render_view.h"

namespace dodoe {

    void BaseSceneFeature::registerPass(RenderGraphBuilder& graph, const RenderFeatureContext& context) const {
        DO_ASSERT(context.view != nullptr, "BaseSceneFeature requires a view");
        DO_ASSERT(context.pass_context != nullptr, "BaseSceneFeature requires pass context");
        RenderPipelinePass::RenderGBufferPass(graph, *context.view, *context.pass_context);
        RenderPipelinePass::RenderDirectionalShadowPass(graph, *context.pass_context);
        RenderPipelinePass::RenderSkyboxPass(graph, *context.pass_context);
    }

    void LightingFeature::registerPass(RenderGraphBuilder& graph, const RenderFeatureContext& context) const {
        DO_ASSERT(context.pass_context != nullptr, "LightingFeature requires pass context");
        RenderPipelinePass::RenderDeferredLightPass(graph, *context.pass_context);
    }

    void PostProcessFeature::registerPass(RenderGraphBuilder& graph, const RenderFeatureContext& context) const {
        DO_ASSERT(context.pass_context != nullptr, "PostProcessFeature requires pass context");
        RenderPipelinePass::RenderToneMappingPass(graph, *context.pass_context);
        RenderPipelinePass::RenderColorGradingPass(graph, *context.pass_context);
        RenderPipelinePass::RenderFxaaPass(graph, *context.pass_context);
    }

    void PresentFeature::registerPass(RenderGraphBuilder& graph, const RenderFeatureContext& context) const {
        DO_ASSERT(context.pass_context != nullptr, "PresentFeature requires pass context");
        RenderPipelinePass::RenderPresentPass(graph, *context.pass_context);
    }

} // dodoe
