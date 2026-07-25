// do@Redlive

#include "gizmo_feature.h"

#ifdef DODOE_EDITOR_ENABLED

#include "runtime/function/render/render_pipeline/passes/render_gizmo_pass.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/shared_render_service.h"
#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

	void GizmoFeature::initialize(SharedRenderService& resources) {
	    auto* cache = resources.getBindingLayoutCache();
	    m_binding_layout = cache->getOrCreate(
	        GfxBindingLayoutDesc().setVisibility(GfxShaderType::All)
	            .addItem(GfxBindingLayoutItem::PushConstants(0, sizeof(float) * 16))
	            .addItem(GfxBindingLayoutItem::ConstantBuffer(0)));
	}

	void GizmoFeature::shutdown() {
	    m_binding_layout.reset();
	}

	void GizmoFeature::collectPasses(PassCollector& collector) {
	    collector.addPass<GizmoPass>();
	}

} // namespace dodoe

#endif // DODOE_EDITOR_ENABLED
