// do@Redlive

#include "gizmo_feature.h"

#ifdef DODOE_EDITOR_ENABLED

#include "runtime/function/render/render_pipeline/passes/render_gizmo_pass.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/shared_render_service.h"
#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

    void GizmoFeature::initialize(SharedRenderService& resources) {
        (void)resources;
        m_binding_layout = GDrawCommandList.createBindingLayout(
            GfxBindingLayoutDesc().setVisibility(GfxShaderType::All)
                .addItem(GfxBindingLayoutItem::PushConstants(0, sizeof(float) * 16))
                .addItem(GfxBindingLayoutItem::ConstantBuffer(0)));
    }

    void GizmoFeature::shutdown() {
        m_binding_layout.reset();
    }

    void GizmoFeature::setupPasses(RenderGraphBuilder& graph, const RenderPassBuildContext& context) const {
        if (!context.view.hasViewFlag(RenderView::kShowEditorPrimitives)) return;

        auto& channel_data = GetGizmoChannel().get<GizmoChannelData>();
        if (!channel_data.has_data || channel_data.commands.empty()) return;

        m_gizmo_pass.build(graph, context);
    }

} // namespace dodoe

#endif // DODOE_EDITOR_ENABLED
