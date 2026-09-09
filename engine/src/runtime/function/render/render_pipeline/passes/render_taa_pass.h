// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/render_pipeline/render_pass.h"
#include "runtime/function/render/render_pipeline/passes/render_pass_blackboard_keys.h"

namespace dodoe {

	class TaaPass : public IRenderPass {
	public:
	    using Produces = TypeList<SceneHdrKey>;
	    using Consumes = TypeList<SceneHdrKey, SceneTexturesKey>;

	    RenderPhase getPhase() const override { return RenderPhase::Taa; }

	    DynamicArray<Size_t> getProducedKeys() const override {
	        return MakeKeyHashes(Produces{});
	    }

	    DynamicArray<Size_t> getConsumedKeys() const override {
	        return MakeKeyHashes(Consumes{});
	    }

	    void build(RenderGraphBuilder& graph,
	               const RenderPassBuildContext& context) override;
	};

} // namespace dodoe
