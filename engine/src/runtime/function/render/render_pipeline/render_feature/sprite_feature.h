// do@Redlive

#pragma once

#include "dopch.h"

#include "render_feature.h"
#include "runtime/function/render/render_pipeline/passes/render_sprite_pass.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

	class SpriteFeature final : public IRenderFeature {
	    GfxBindingLayoutHandle m_bindless_binding_layout{};
	    GfxBindingLayoutHandle m_array_binding_layout{};
	    GfxInputLayoutHandle m_input_layout{};

	public:
	    void initialize(SharedRenderService& resources) override;
	    void shutdown() override;

	    void collectPasses(PassCollector& collector) override;

	    [[nodiscard]] GfxBindingLayoutHandle getBindlessBindingLayout() const { return m_bindless_binding_layout; }
	    [[nodiscard]] GfxBindingLayoutHandle getArrayBindingLayout() const { return m_array_binding_layout; }
	    [[nodiscard]] GfxInputLayoutHandle getInputLayout() const { return m_input_layout; }
	};

} // namespace dodoe
