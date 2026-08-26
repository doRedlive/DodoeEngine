// do@Redlive

#pragma once

#include "dopch.h"

#include "render_feature.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

	class ImGuiFeature final : public IRenderFeature {
	    GfxTextureHandle m_font_texture{};
	    GfxBufferHandle m_imgui_cb{};
	    GfxBindingLayoutHandle m_binding_layout{};
	    GfxBindingSetHandle m_font_binding_set{};
	    GfxInputLayoutHandle m_input_layout{};

	public:
	    void initialize(SharedRenderService& resources) override;
	    void shutdown() override;
	    void registerGraphImports(RenderGraphImportRegistry& imports,
	                              const RenderView& view) override;

	    void collectPasses(PassCollector& collector) override;

	private:
#ifdef DODOE_DEBUG_ENABLED
	    void setupViewports(SharedRenderService& resources);
#endif
	};

} // namespace dodoe
