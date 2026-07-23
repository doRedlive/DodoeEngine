// do@Redlive

#pragma once

#include "dopch.h"

#include "../render_pass.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class SpritePass : public IRenderPass {
        GfxBindingLayoutHandle m_binding_layout{};
        GfxBindingLayoutHandle m_traditional_tex_layout{};

    public:
        SpritePass() = default;
        SpritePass(GfxBindingLayoutHandle binding_layout,
                   GfxBindingLayoutHandle traditional_tex_layout)
            : m_binding_layout(binding_layout)
            , m_traditional_tex_layout(traditional_tex_layout) {}

        void build(RenderGraphBuilder& graph,
                   const RenderPassBuildContext& context) override;
    };

} // namespace dodoe
