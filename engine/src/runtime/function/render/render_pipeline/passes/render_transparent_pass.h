// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/render_pipeline/passes/render_opaque_pass.h"

namespace dodoe {

    class LitMeshProcessor;

    class TransparentPass final : public OpaquePass {
    public:
        using Produces = TypeList<>;
        using Consumes = TypeList<SceneHdrKey, SceneTexturesKey, ShadowMapKey>;

        explicit TransparentPass(const LitMeshProcessor* processor = nullptr)
            : OpaquePass(processor) {}

        RenderPhase getPhase() const override { return RenderPhase::Transparent; }

        DynamicArray<Size_t> getConsumedKeys() const override {
            return MakeKeyHashes(Consumes{});
        }

        void build(RenderGraphBuilder& graph,
                   const RenderPassBuildContext& context) override;
    };

} // namespace dodoe
