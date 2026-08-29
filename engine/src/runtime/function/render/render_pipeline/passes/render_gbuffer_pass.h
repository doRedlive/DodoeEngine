// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/render_pipeline/render_pass.h"
#include "runtime/function/render/render_pipeline/passes/render_pass_blackboard_keys.h"

namespace dodoe {

	class LitMeshProcessor;

	class GBufferPass : public IRenderPass {
	public:
	    using Produces = TypeList<SceneTexturesKey>;
	    using Consumes = TypeList<>;

	    explicit GBufferPass(const LitMeshProcessor* processor = nullptr)
	        : m_mesh_processor(processor) {}

	    RenderPhase getPhase() const override { return RenderPhase::Opaque; }

	    DynamicArray<Size_t> getProducedKeys() const override {
	        return MakeKeyHashes(Produces{});
	    }

	    void build(RenderGraphBuilder& graph,
	               const RenderPassBuildContext& context) override;

	private:
	    const LitMeshProcessor* m_mesh_processor;
	};

} // namespace dodoe
