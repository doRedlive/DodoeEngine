// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/render_pipeline/render_pass.h"

namespace dodoe {

    class SharedRenderService;

    class IRenderFeature {
    public:
        virtual ~IRenderFeature() = default;

        virtual void initialize(SharedRenderService& resources) {}
        virtual void onResize(UInt32 width, UInt32 height) {}
        virtual void setupPasses(RenderGraphBuilder& graph, const RenderPassBuildContext& context) const = 0;
        virtual void shutdown() {}
    };

} // dodoe
