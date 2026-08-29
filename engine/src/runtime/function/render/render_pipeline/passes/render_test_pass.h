// do@Redlive

#pragma once

#include "dopch.h"

#include "../render_pass.h"
#include "render_pass_blackboard_keys.h"

namespace dodoe {

	class TestPass : public IRenderPass {
	public:
	    using Produces = TypeList<SceneColorKey>;
	    using Consumes = TypeList<>;

	    RenderPhase getPhase() const override { return RenderPhase::DebugUI; }

	    DynamicArray<Size_t> getProducedKeys() const override {
	        return MakeKeyHashes(Produces{});
	    }

	    void build(RenderGraphBuilder& graph,
	               const RenderPassBuildContext& context) override;
	};

} // namespace dodoe
