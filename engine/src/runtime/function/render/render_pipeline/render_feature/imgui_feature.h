// do@Redlive

#pragma once

#include "imgui_render_resources.h"
#include "render_builtin_features.h"

namespace dodoe {

    class ImGuiFeature final : public IRenderFeature {
        mutable ImGuiRenderResources m_resources{};

    public:
        ~ImGuiFeature() override { m_resources.reset(); }
        void registerPass(RenderGraphBuilder& graph, const RenderFeatureContext& context) const override;
    };

} // dodoe
