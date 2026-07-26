// do@Redlive

#pragma once

#include "dopch.h"

#include "../render_pass.h"
#include "render_pass_blackboard_keys.h"

namespace dodoe {

	class SkyboxPass : public IRenderPass {
	public:
		SkyboxPass() = default;
	    using Produces = TypeList<SceneHdrKey>;
	    using Consumes = TypeList<SceneTexturesKey>;

	    RenderPhase getPhase() const override { return RenderPhase::Skybox; }

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
