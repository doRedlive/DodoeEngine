// do@Redlive

#pragma once

#include "dopch.h"

#include "../render_pass.h"
#include "render_pass_blackboard_keys.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

	class SpritePass : public IRenderPass {
	    GfxBindingLayoutHandle m_cb_binding_layout{};
	    GfxBindingLayoutHandle m_sampler_binding_layout{};
	    GfxBindingLayoutHandle m_material_binding_layout{};
	    GfxInputLayoutHandle m_input_layout{};

	public:
	    using Produces = TypeList<SceneColorKey>;
	    using Consumes = TypeList<>;

	    SpritePass() = default;
	    SpritePass(GfxBindingLayoutHandle cb_binding_layout,
	               GfxBindingLayoutHandle sampler_binding_layout,
	               GfxBindingLayoutHandle material_binding_layout,
	               GfxInputLayoutHandle input_layout)
	        : m_cb_binding_layout(std::move(cb_binding_layout))
	        , m_sampler_binding_layout(std::move(sampler_binding_layout))
	        , m_material_binding_layout(std::move(material_binding_layout))
	        , m_input_layout(std::move(input_layout)) {}

	    RenderPhase getPhase() const override { return RenderPhase::Sprite; }

	    DynamicArray<Size_t> getProducedKeys() const override { return MakeKeyHashes(Produces{}); }

	    void build(RenderGraphBuilder& graph,
	               const RenderPassBuildContext& context) override;
	};

} // namespace dodoe
