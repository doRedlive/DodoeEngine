// do@Redlive

#include "test_feature.h"

#include "runtime/function/render/render_pipeline/passes/render_pipeline_passes.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"

namespace dodoe {

    void TestFeature::registerPass(RenderGraphBuilder& graph, const RenderFeatureContext& context) const {
        DO_ASSERT(context.pass_context != nullptr, "TestFeature requires pass context");
        RenderPipelinePass::RenderTestPass(graph, *context.pass_context);
    }

} // dodoe
