// do@Redlive

#pragma once

#include "dopch.h"

#include "../render_pass.h"

namespace dodoe {

    class GBufferPass : public IRenderPass {
    public:
        void build(RenderGraphBuilder& graph,
                   const RenderPassBuildContext& context) override;
    };

    class DirectionalShadowPass : public IRenderPass {
    public:
        void build(RenderGraphBuilder& graph,
                   const RenderPassBuildContext& context) override;
    };

} // namespace dodoe
