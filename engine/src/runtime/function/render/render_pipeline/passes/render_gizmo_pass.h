// do@Redlive

#pragma once

#include "dopch.h"

#include "../render_pass.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

#ifdef DODOE_EDITOR_ENABLED

	class GizmoPass : public IRenderPass {
	    GfxBindingLayoutHandle m_binding_layout{};
	    GfxInputLayoutHandle m_input_layout{};

	public:
	    using Produces = TypeList<>;
	    using Consumes = TypeList<>;

	    RenderPhase getPhase() const override { return RenderPhase::EditorGizmo; }

	    GizmoPass() = default;
	    explicit GizmoPass(GfxBindingLayoutHandle binding_layout, GfxInputLayoutHandle input_layout)
	        : m_binding_layout(std::move(binding_layout)), m_input_layout(std::move(input_layout)) {}

	    void build(RenderGraphBuilder& graph,
	               const RenderPassBuildContext& context) override;
	};

#endif

} // namespace dodoe
