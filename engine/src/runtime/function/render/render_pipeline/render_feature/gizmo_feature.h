// do@Redlive

#pragma once

#include "dopch.h"

#ifdef DODOE_EDITOR_ENABLED

#include "render_feature.h"
#include "gizmo_render_resource.h"

namespace dodoe {

    class GizmoFeature final : public IRenderFeature {
        mutable GizmoRenderResource m_resources{};

    public:
        ~GizmoFeature() override { m_resources.reset(); }
        void registerPass(RenderGraphBuilder& graph, const RenderFeatureContext& context) const override;
    };

} // namespace dodoe

#endif // DODOE_EDITOR_ENABLED
