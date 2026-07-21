// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

    class RenderGraphBuilder;
    class RenderView;
    struct RenderPassContext;

    class IRenderPass {
    public:
        virtual ~IRenderPass() = default;

        virtual void build(RenderGraphBuilder& graph,
                           const RenderPassContext& context,
                           const RenderView& view) = 0;
    };

} // namespace dodoe
