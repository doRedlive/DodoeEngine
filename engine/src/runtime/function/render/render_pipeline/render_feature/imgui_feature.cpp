// do@Redlive

#include "imgui_feature.h"

#include "runtime/function/render/render_pipeline/passes/render_pipeline_passes.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"

namespace dodoe {

    void ImGuiFeature::registerPass(RenderGraphBuilder& graph, const RenderFeatureContext& context) const {
        DO_ASSERT(context.pass_context != nullptr, "ImGuiFeature requires pass context");
        RenderPipelinePass::RenderImGuiPass(graph, *context.pass_context, m_resources);
    }

} // dodoe
