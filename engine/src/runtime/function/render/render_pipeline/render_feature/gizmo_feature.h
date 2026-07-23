// do@Redlive

#pragma once

#include "dopch.h"

#ifdef DODOE_EDITOR_ENABLED

#include "render_feature.h"
#include "runtime/function/render/render_pipeline/passes/render_gizmo_pass.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class GizmoFeature final : public IRenderFeature {
        GfxBindingLayoutHandle m_binding_layout{};
        GizmoPass m_gizmo_pass{};

    public:
        void initialize(SharedRenderService& resources) override;
        void shutdown() override;

        void setupPasses(RenderGraphBuilder& graph, const RenderPassBuildContext& context) const override;

        [[nodiscard]] GfxBindingLayoutHandle getBindingLayout() const { return m_binding_layout; }
    };

} // namespace dodoe

#endif // DODOE_EDITOR_ENABLED
