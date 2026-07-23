// do@Redlive

#pragma once

#include "dopch.h"

#include "../render_pass.h"

namespace dodoe {

    class ImGuiPass : public IRenderPass {
    public:
        void build(RenderGraphBuilder& graph,
                   const RenderPassBuildContext& context) override;
    };

} // namespace dodoe
