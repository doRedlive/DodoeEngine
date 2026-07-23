// do@Redlive

#pragma once

#include "dopch.h"

#include "../render_pass.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class DeferredLightPass : public IRenderPass {
        GfxBufferHandle m_constant_buffer{};

    public:
        DeferredLightPass() = default;
        explicit DeferredLightPass(GfxBufferHandle constant_buffer)
            : m_constant_buffer(constant_buffer) {}

        void build(RenderGraphBuilder& graph,
                   const RenderPassBuildContext& context) override;
    };

} // namespace dodoe
