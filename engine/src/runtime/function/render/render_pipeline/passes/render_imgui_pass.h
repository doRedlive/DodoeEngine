// do@Redlive

#pragma once

#include "dopch.h"

#include "../render_pass.h"
#include "render_pass_blackboard_keys.h"

namespace dodoe {

	class ImGuiPass : public IRenderPass {
	    GfxBindingLayoutHandle m_binding_layout{};
	    GfxBindingSetHandle m_font_binding_set{};
	    GfxInputLayoutHandle m_input_layout{};

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
	              GfxInputLayoutHandle input_layout)
	        : m_binding_layout(std::move(binding_layout))
	        , m_font_binding_set(std::move(font_binding_set))
	        , m_input_layout(std::move(input_layout)) {}

	    void build(RenderGraphBuilder& graph,
	               const RenderPassBuildContext& context) override;
	};

} // namespace dodoe
