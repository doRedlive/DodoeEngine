// do@Redlive

#pragma once

#include "dopch.h"

#include "render_feature.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class SpriteFeature final : public IRenderFeature {
        GfxBindingLayoutHandle m_binding_layout{};
        GfxBindingLayoutHandle m_traditional_tex_layout{};

    public:
        void initialize(SharedRenderService& resources) override;
        void shutdown() override;

        void registerPass(RenderGraphBuilder& graph, const RenderPassBuildContext& context) const override;

        [[nodiscard]] GfxBindingLayoutHandle getBindingLayout() const { return m_binding_layout; }
        [[nodiscard]] GfxBindingLayoutHandle getTraditionalTextureLayout() const { return m_traditional_tex_layout; }
    };

} // namespace dodoe
