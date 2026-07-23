// do@Redlive

#pragma once

#include "dopch.h"

#include "render_feature.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class ImGuiFeature final : public IRenderFeature {
        GfxTextureHandle m_font_texture{};
        GfxBufferHandle m_constant_buffer{};
        GfxBindingLayoutHandle m_binding_layout{};

    public:
        void initialize(SharedRenderService& resources) override;
        void shutdown() override;

        void registerPass(RenderGraphBuilder& graph, const RenderPassBuildContext& context) const override;

        [[nodiscard]] GfxTextureHandle getFontTexture() const { return m_font_texture; }
        [[nodiscard]] GfxBufferHandle getConstantBuffer() const { return m_constant_buffer; }
        [[nodiscard]] GfxBindingLayoutHandle getBindingLayout() const { return m_binding_layout; }
    };

} // namespace dodoe
