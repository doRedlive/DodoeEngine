// do@Redlive

#include "managed_render_pass.h"

#include "srp_bridge.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"

namespace dodoe {

    void ManagedRenderPass::build(RenderGraphBuilder& graph, const RenderPassBuildContext& context) {
        SrpBridge::Self().featureAddPasses(m_feature_id, graph, context);
    }

} // dodoe
