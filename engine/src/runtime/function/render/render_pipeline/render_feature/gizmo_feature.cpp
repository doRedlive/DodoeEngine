// do@Redlive

#include "gizmo_feature.h"

#ifdef DODOE_EDITOR_ENABLED

#include "runtime/function/render/render_pipeline/passes/render_pipeline_passes.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"

namespace dodoe {

    void GizmoFeature::registerPass(RenderGraphBuilder& graph, const RenderFeatureContext& context) const {
        if (!context.view || !context.pass_context) return;

        if (!context.view->hasViewFlag(RenderView::kShowEditorPrimitives)) return;

        auto& channel_data = GetGizmoChannel().get<GizmoChannelData>();
        if (!channel_data.has_data || channel_data.commands.empty()) return;

        DO_ASSERT(context.pass_context->isValid(), "GizmoFeature requires pass context");
        RenderPipelinePass::RenderGizmoPass(graph, *context.pass_context, m_resources);
    }

} // namespace dodoe

#endif // DODOE_EDITOR_ENABLED
