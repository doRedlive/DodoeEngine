// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/render_pipeline/render_pass.h"
#include "runtime/function/render/render_pipeline/passes/render_pass_blackboard_keys.h"

namespace dodoe {

    class MeshPassProcessor;

	class ShadowPass : public IRenderPass {
	public:
	    using Produces = TypeList<ShadowMapKey>;
	    using Consumes = TypeList<SceneTexturesKey>;

        explicit ShadowPass(const MeshPassProcessor* processor = nullptr)
	        : m_mesh_processor(processor) {}

	    RenderPhase getPhase() const override { return RenderPhase::Shadow; }

	    DynamicArray<Size_t> getProducedKeys() const override {
	        return MakeKeyHashes(Produces{});
	    }

	    DynamicArray<Size_t> getConsumedKeys() const override {
	        return MakeKeyHashes(Consumes{});
	    }

	    void build(RenderGraphBuilder& graph,
	               const RenderPassBuildContext& context) override;

	private:
        const MeshPassProcessor* m_mesh_processor;
	};

} // namespace dodoe
