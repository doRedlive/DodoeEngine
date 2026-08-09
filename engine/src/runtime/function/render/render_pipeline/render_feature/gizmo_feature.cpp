// do@Redlive

#include "gizmo_feature.h"

#ifdef DODOE_EDITOR_ENABLED

#include "runtime/function/render/render_pipeline/passes/render_gizmo_pass.h"
#include "runtime/core/channel/gizmo_channel.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/render_service/input_layout_cache.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

	void GizmoFeature::initialize(SharedRenderService& resources) {
	    auto* cache = resources.getBindingLayoutCache();
	    m_binding_layout = cache->getOrCreate(
	        GfxBindingLayoutDesc().setVisibility(GfxShaderType::All)
	            .setRegisterSpaceIsDescriptorSet(true)
	            .addItem(GfxBindingLayoutItem::PushConstants(0, sizeof(float) * 16 + sizeof(float) * 4)));

	    if (auto* input_layout_cache = resources.getInputLayoutCache()) {
	        const DynamicArray<GfxVertexAttributeDesc> attributes = {
	            GfxVertexAttributeDesc().setName("a_Position").setFormat(GfxFormat::RGB32_FLOAT).setOffset(0).setElementStride(sizeof(GizmoVertex)),
	            GfxVertexAttributeDesc().setName("a_Color").setFormat(GfxFormat::RGBA32_FLOAT).setOffset(sizeof(Vector3f)).setElementStride(sizeof(GizmoVertex)),
	        };
	        m_input_layout = input_layout_cache->getOrCreate(
	            attributes, resources.getShaderLibrary()->getGizmoVertexShader());
	    }
	}

	void GizmoFeature::shutdown() {
	    m_binding_layout = nullptr;
	    m_input_layout = nullptr;
	}

	void GizmoFeature::collectPasses(PassCollector& collector) {
	    collector.addPass<GizmoPass>(m_binding_layout, m_input_layout);
	}

} // namespace dodoe

#endif // DODOE_EDITOR_ENABLED
