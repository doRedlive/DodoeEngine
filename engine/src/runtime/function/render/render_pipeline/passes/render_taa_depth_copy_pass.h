// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/render_pipeline/render_pass.h"
#include "runtime/function/render/render_pipeline/passes/render_pass_blackboard_keys.h"

namespace dodoe {

	class TaaDepthCopyPass : public IRenderPass {
	public:
	    using Consumes = TypeList<SceneTexturesKey>;

	    RenderPhase getPhase() const override { return RenderPhase::Resolve; }

	    DynamicArray<Size_t> getProducedKeys() const override {
	        return {};
	    }

	    DynamicArray<Size_t> getConsumedKeys() const override {
	        return MakeKeyHashes(Consumes{});
	    }

	    void build(RenderGraphBuilder& graph,
	               const RenderPassBuildContext& context) override;
	};

} // namespace dodoe
