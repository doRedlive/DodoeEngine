// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/render_pipeline/render_pass_context.h"

namespace dodoe {

    class RenderGraphBuilder;
    class RenderView;

    struct RenderFeatureContext {
        const RenderView* view{nullptr};
        const RenderPassContext* pass_context{nullptr};
    };

    class IRenderFeature {
    public:
        virtual ~IRenderFeature() = default;
        virtual void registerPass(RenderGraphBuilder& graph, const RenderFeatureContext& context) const = 0;
    };

} // dodoe
