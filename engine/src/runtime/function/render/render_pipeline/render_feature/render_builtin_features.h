// do@Redlive

#pragma once

#include "render_feature.h"

namespace dodoe {

    class BaseSceneFeature final : public IRenderFeature {
    public:
        void registerPass(RenderGraphBuilder& graph, const RenderFeatureContext& context) const override;
    };

    class LightingFeature final : public IRenderFeature {
    public:
        void registerPass(RenderGraphBuilder& graph, const RenderFeatureContext& context) const override;
    };

    class PostProcessFeature final : public IRenderFeature {
    public:
        void registerPass(RenderGraphBuilder& graph, const RenderFeatureContext& context) const override;
    };

    class PresentFeature final : public IRenderFeature {
    public:
        void registerPass(RenderGraphBuilder& graph, const RenderFeatureContext& context) const override;
    };

} // dodoe
