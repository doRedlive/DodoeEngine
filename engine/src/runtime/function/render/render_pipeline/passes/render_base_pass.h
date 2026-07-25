// do@Redlive

#pragma once

#include "dopch.h"

#include "../render_pass.h"
#include "render_pass_blackboard_keys.h"

namespace dodoe {

	class GBufferMeshProcessor;
	class DirectionalShadowMeshProcessor;

	class GBufferPass : public IRenderPass {
	public:
	    using Produces = TypeList<SceneTexturesKey>;
	    using Consumes = TypeList<>;

	    explicit GBufferPass(const GBufferMeshProcessor* processor = nullptr)
	        : m_mesh_processor(processor) {}

	    RenderPhase getPhase() const override { return RenderPhase::GBuffer; }

	    DynamicArray<Size_t> getProducedKeys() const override {
	        return MakeKeyHashes(Produces{});
	    }

	    void build(RenderGraphBuilder& graph,
	               const RenderPassBuildContext& context) override;

	private:
	    const GBufferMeshProcessor* m_mesh_processor;
	};

	class DirectionalShadowPass : public IRenderPass {
	public:
	    using Produces = TypeList<ShadowMapKey>;
	    using Consumes = TypeList<SceneTexturesKey>;

	    explicit DirectionalShadowPass(const DirectionalShadowMeshProcessor* processor = nullptr)
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
	    const DirectionalShadowMeshProcessor* m_mesh_processor;
	};

} // namespace dodoe
