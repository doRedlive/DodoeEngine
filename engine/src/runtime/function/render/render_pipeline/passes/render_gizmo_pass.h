// do@Redlive

#pragma once

#include "dopch.h"

#include "../render_pass.h"

namespace dodoe {

#ifdef DODOE_EDITOR_ENABLED

    class GizmoPass : public IRenderPass {
    public:
        void build(RenderGraphBuilder& graph,
                   const RenderPassBuildContext& context) override;
    };

#endif

} // namespace dodoe
