// do@Redlive

#pragma once

#include "dopch.h"

#include "render_feature.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

	class ImGuiFeature final : public IRenderFeature {
	    GfxTextureHandle m_font_texture{};
	    GfxBindingLayoutHandle m_binding_layout{};
	    GfxBindingLayoutHandle m_push_layout{};
	    GfxBindingSetHandle m_font_binding_set{};
	    GfxInputLayoutHandle m_input_layout{};

	public:
	    void initialize(SharedRenderService& resources) override;
	    void shutdown() override;
	    void registerGraphImports(RenderGraphImportRegistry& imports,
	                              const RenderView& view) override;

	    void collectPasses(PassCollector& collector) override;

	    [[nodiscard]] GfxTextureHandle getFontTexture() const { return m_font_texture; }
	    [[nodiscard]] GfxBindingLayoutHandle getBindingLayout() const { return m_binding_layout; }
	    [[nodiscard]] GfxBindingLayoutHandle getPushLayout() const { return m_push_layout; }
	    [[nodiscard]] GfxBindingSetHandle getFontBindingSet() const { return m_font_binding_set; }
	    [[nodiscard]] GfxInputLayoutHandle getInputLayout() const { return m_input_layout; }
	};

} // namespace dodoe
