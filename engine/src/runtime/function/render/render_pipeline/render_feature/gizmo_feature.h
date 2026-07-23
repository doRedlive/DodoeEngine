// do@Redlive

#pragma once

#include "dopch.h"

#ifdef DODOE_EDITOR_ENABLED

#include "render_feature.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class GizmoFeature final : public IRenderFeature {
        GfxBindingLayoutHandle m_binding_layout{};

    public:
        void initialize(SharedRenderService& resources) override;
        void shutdown() override;

        void registerPass(RenderGraphBuilder& graph, const RenderPassBuildContext& context) const override;

        [[nodiscard]] GfxBindingLayoutHandle getBindingLayout() const { return m_binding_layout; }
    };

} // namespace dodoe

#endif // DODOE_EDITOR_ENABLED
