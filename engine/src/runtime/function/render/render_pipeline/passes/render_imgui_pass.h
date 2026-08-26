// do@Redlive

#pragma once

#include "dopch.h"

#include "../render_pass.h"
#include "render_pass_blackboard_keys.h"

#ifdef DODOE_DEBUG_ENABLED
#include "runtime/function/ui/imgui/imgui_draw_renderer.h"
#endif

namespace dodoe {

#ifdef DODOE_DEBUG_ENABLED
	class ImGuiPass : public IRenderPass {
	    GfxBindingLayoutHandle m_binding_layout{};
	    GfxBindingSetHandle m_font_binding_set{};
	    GfxInputLayoutHandle m_input_layout{};
	    ImGuiDrawRenderer m_renderer{};

	public:
	    using Produces = TypeList<ImGuiColorKey>;
	    using Consumes = TypeList<>;

	    RenderPhase getPhase() const override { return RenderPhase::DebugUI; }

	    DynamicArray<Size_t> getProducedKeys() const override {
	        return MakeKeyHashes(Produces{});
	    }

	    ImGuiPass() = default;
	    ImGuiPass(GfxBindingLayoutHandle binding_layout,
	              GfxBindingSetHandle font_binding_set,
	              GfxInputLayoutHandle input_layout,
	              GfxTextureHandle font_texture,
	              GfxBufferHandle font_binding_set_cb)
	        : m_binding_layout(std::move(binding_layout))
	        , m_font_binding_set(std::move(font_binding_set))
	        , m_input_layout(std::move(input_layout))
	        , m_renderer(m_binding_layout, m_font_binding_set, m_input_layout,
	                    std::move(font_texture), std::move(font_binding_set_cb))
	        {}

	    void build(RenderGraphBuilder& graph,
	               const RenderPassBuildContext& context) override;
	};
#endif

} // namespace dodoe
