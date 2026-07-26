// do@Redlive

#pragma once

#include "dopch.h"

#include "../render_pass.h"
#include "render_pass_blackboard_keys.h"

namespace dodoe {

	class PresentPass : public IRenderPass {
	public:
	    PresentPass() = default;

	    using Produces = TypeList<>;
	    using Consumes = TypeList<SceneColorKey>;

	    RenderPhase getPhase() const override { return RenderPhase::Present; }

	    DynamicArray<Size_t> getConsumedKeys() const override {
	        return MakeKeyHashes(Consumes{});
	    }

	    void build(RenderGraphBuilder& graph,
	               const RenderPassBuildContext& context) override;
	};

} // namespace dodoe
