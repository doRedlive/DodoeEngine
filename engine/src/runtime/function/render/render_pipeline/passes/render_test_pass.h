// do@Redlive

#pragma once

#include "dopch.h"

#include "../render_pass.h"

namespace dodoe {

	class TestPass : public IRenderPass {
	public:
	    using Produces = TypeList<>;
	    using Consumes = TypeList<>;

	    RenderPhase getPhase() const override { return RenderPhase::Forward; }

	    void build(RenderGraphBuilder& graph,
	               const RenderPassBuildContext& context) override;
	};

} // namespace dodoe
