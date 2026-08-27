// do@Redlive

#pragma once

#include "dopch.h"

#include "../render_pass.h"
#include "render_pass_blackboard_keys.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

	class DeferredLightPass : public IRenderPass {
	public:
	    using Produces = TypeList<>;
	    using Consumes = TypeList<SceneTexturesKey, ShadowMapKey, SceneHdrKey>;

	    RenderPhase getPhase() const override { return RenderPhase::Lighting; }

	    DynamicArray<Size_t> getConsumedKeys() const override {
	        return MakeKeyHashes(Consumes{});
	    }

	    void build(RenderGraphBuilder& graph,
	               const RenderPassBuildContext& context) override;
	};

} // namespace dodoe
