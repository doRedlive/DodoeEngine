// do@Redlive

#pragma once

#include "render_feature.h"
#include "deferred_light_render_resource.h"

namespace dodoe {

    class BaseSceneFeature final : public IRenderFeature {
    public:
        void registerPass(RenderGraphBuilder& graph, const RenderFeatureContext& context) const override;
    };

    class LightingFeature final : public IRenderFeature {
        mutable DeferredLightRenderResource m_resources{};

    public:
        ~LightingFeature() override { m_resources.reset(); }
        void registerPass(RenderGraphBuilder& graph, const RenderFeatureContext& context) const override;
    };

    class PostProcessFeature final : public IRenderFeature {
    public:
        void registerPass(RenderGraphBuilder& graph, const RenderFeatureContext& context) const override;
    };

    class PostProcess2DFeature final : public IRenderFeature {
    public:
        void registerPass(RenderGraphBuilder& graph, const RenderFeatureContext& context) const override;
    };

    class PresentFeature final : public IRenderFeature {
    public:
        void registerPass(RenderGraphBuilder& graph, const RenderFeatureContext& context) const override;
    };

} // dodoe
