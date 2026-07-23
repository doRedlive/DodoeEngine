// do@Redlive

#include "test_feature.h"

#include "runtime/function/render/render_pipeline/passes/render_test_pass.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"

namespace dodoe {

    void TestFeature::registerPass(RenderGraphBuilder& graph, const RenderFeatureContext& context) const {
        DO_ASSERT(context.pass_context != nullptr, "TestFeature requires pass context");
        RenderPassBuildContext build_ctx{*context.pass_context, *context.view};
        TestPass{}.build(graph, build_ctx);
    }

} // namespace dodoe
