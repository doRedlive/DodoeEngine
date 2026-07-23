// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

    class RenderGraphBuilder;
    class RenderView;
    struct RenderPassContext;

    struct RenderPassBuildContext {
        const RenderPassContext& pass_context;
        const RenderView& view;
    };

    class IRenderPass {
    public:
        virtual ~IRenderPass() = default;

        virtual void build(RenderGraphBuilder& graph,
                           const RenderPassBuildContext& context) = 0;
    };

} // namespace dodoe
