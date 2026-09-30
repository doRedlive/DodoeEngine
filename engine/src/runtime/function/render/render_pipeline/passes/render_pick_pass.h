// do@Redlive

#pragma once

#include "dopch.h"

#include "../render_pass.h"
#include "render_pass_blackboard_keys.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

#ifdef DODOE_EDITOR_ENABLED

	class RenderGraphPassContext;
	class DrawCommandList;

	class EditorPickPass : public IRenderPass {
	public:
	    using Produces = TypeList<>;
	    using Consumes = TypeList<SceneTexturesKey>;

	    RenderPhase getPhase() const override { return RenderPhase::DebugUI; }

	    DynamicArray<Size_t> getConsumedKeys() const override {
	        return MakeKeyHashes(Consumes{});
	    }

	    void build(RenderGraphBuilder& graph,
	               const RenderPassBuildContext& context) override;

	private:
	    struct PickPassParameters {
	        RenderGraphTextureHandle depth{};
	        RenderGraphBufferHandle instance_buffer{};
	        RenderGraphTextureHandle pick_target{};
	        UInt32 pick_x{0};
	        UInt32 pick_y{0};
	    };

	    void ensureCpuResources(const RenderPassBuildContext& context);
	    Bool ensureGpuResources(const RenderGraphPassContext& ctx, DrawCommandList& command_list);
	    void resolvePendingCopy();
	    void publishResult(UInt64 sequence, UInt64 entity_uuid);
	    void executePick(const PickPassParameters& parameters,
	                     const RenderGraphPassContext& ctx,
	                     DrawCommandList& command_list);

	    GfxBindingLayoutHandle m_view_binding_layout{};
	    GfxBindingLayoutHandle m_primitive_binding_layout{};
	    GfxInputLayoutHandle m_input_layout{};
	    GfxBufferHandle m_view_cb{};
	    GfxBufferHandle m_primitive_cb{};
	    GfxBindingSetHandle m_view_binding_set{};
	    GfxBindingSetHandle m_primitive_binding_set{};
	    GfxStagingTextureHandle m_staging_texture{};
	    DynamicArray<UInt64> m_pick_ids{};
	    UInt64 m_consumed_sequence{0};
	    UInt64 m_pending_sequence{0};
	    UInt32 m_active_x{0};
	    UInt32 m_active_y{0};
	    UInt32 m_resolve_delay{0};
	    Bool m_copy_in_flight{false};
	};

#endif // DODOE_EDITOR_ENABLED

} // namespace dodoe
