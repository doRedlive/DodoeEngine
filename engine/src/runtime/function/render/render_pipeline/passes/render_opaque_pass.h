// do@Redlive

#pragma once

#include "dopch.h"

#include "../render_pass.h"
#include "render_pass_blackboard_keys.h"

namespace dodoe {

    class LitMeshProcessor;

    class OpaquePass : public IRenderPass {
    public:
        using Produces = TypeList<SceneHdrKey, SceneTexturesKey>;
        using Consumes = TypeList<>;

        explicit OpaquePass(const LitMeshProcessor* processor = nullptr)
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

    class TransparentPass : public IRenderPass {
    public:
        using Produces = TypeList<>;
        using Consumes = TypeList<SceneHdrKey, SceneTexturesKey, ShadowMapKey>;

        explicit TransparentPass(const LitMeshProcessor* processor = nullptr)
            : m_mesh_processor(processor) {}

        RenderPhase getPhase() const override { return RenderPhase::Transparent; }

        DynamicArray<Size_t> getConsumedKeys() const override {
            return MakeKeyHashes(Consumes{});
        }

        void build(RenderGraphBuilder& graph,
                   const RenderPassBuildContext& context) override;

    private:
        const LitMeshProcessor* m_mesh_processor;
    };

} // namespace dodoe
