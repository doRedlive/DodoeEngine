// do@Redlive

#pragma once

#include "render_builtin_features.h"
#include "sprite_render_resources.h"

namespace dodoe {

    class SpriteFeature final : public IRenderFeature {
        mutable SpriteRenderResources m_resources{};

    public:
        ~SpriteFeature() override { m_resources.reset(); }
        void registerPass(RenderGraphBuilder& graph, const RenderFeatureContext& context) const override;
    };

} // dodoe
