// do@Redlive

#include "sprite_feature.h"

#include "runtime/function/render/render_pipeline/passes/render_pipeline_passes.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"

namespace dodoe {

    void SpriteFeature::registerPass(RenderGraphBuilder& graph, const RenderFeatureContext& context) const {
        DO_ASSERT(context.view != nullptr, "SpriteFeature requires view");
        DO_ASSERT(context.pass_context != nullptr, "SpriteFeature requires pass context");
        RenderPipelinePass::RenderSpritePass(graph, *context.view, *context.pass_context, m_resources);
    }

} // dodoe
