// do@Redlive

#pragma once

#include "dopch.h"

#include "render_feature.h"

namespace dodoe {

    class TestFeature : public IRenderFeature {
    public:
        ~TestFeature() override = default;

        void registerPass(RenderGraphBuilder& graph, const RenderFeatureContext& context) const override;
    };

} // dodoe