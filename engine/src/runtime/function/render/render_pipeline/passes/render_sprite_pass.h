// do@Redlive

#pragma once

#include "dopch.h"

#include "../render_pass.h"
#include "render_pass_blackboard_keys.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

	class SpritePass : public IRenderPass {
	    GfxBindingLayoutHandle m_bindless_binding_layout{};
	    GfxBindingLayoutHandle m_array_binding_layout{};
	    GfxInputLayoutHandle m_input_layout{};

	public:
	    using Produces = TypeList<SceneColorKey>;
	    using Consumes = TypeList<>;

	    SpritePass() = default;
	    SpritePass(GfxBindingLayoutHandle bindless_binding_layout,
	               GfxBindingLayoutHandle array_binding_layout,
	               GfxInputLayoutHandle input_layout)
	        : m_bindless_binding_layout(std::move(bindless_binding_layout))
	        , m_array_binding_layout(std::move(array_binding_layout))
	        , m_input_layout(std::move(input_layout)) {}

	    RenderPhase getPhase() const override { return RenderPhase::Sprite; }

	    DynamicArray<Size_t> getProducedKeys() const override { return MakeKeyHashes(Produces{}); }

	    void build(RenderGraphBuilder& graph,
	               const RenderPassBuildContext& context) override;
	};

} // namespace dodoe
