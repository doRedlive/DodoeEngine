// do@Redlive

#pragma once

#include "dopch.h"

#include "../render_pass.h"
#include "render_pass_blackboard_keys.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    class UIPass : public IRenderPass {
        GfxBindingLayoutHandle m_view_binding_layout{};
        GfxBindingLayoutHandle m_bindless_binding_layout{};
        GfxBindingLayoutHandle m_material_binding_layout{};
        GfxInputLayoutHandle m_input_layout{};

    public:
        using Produces = TypeList<SceneColorKey>;
        using Consumes = TypeList<>;

        UIPass() = default;
        UIPass(GfxBindingLayoutHandle view_binding_layout,
               GfxBindingLayoutHandle bindless_binding_layout,
               GfxBindingLayoutHandle material_binding_layout,
               GfxInputLayoutHandle input_layout)
            : m_view_binding_layout(std::move(view_binding_layout))
            , m_bindless_binding_layout(std::move(bindless_binding_layout))
            , m_material_binding_layout(std::move(material_binding_layout))
            , m_input_layout(std::move(input_layout)) {}

        RenderPhase getPhase() const override { return RenderPhase::UI; }

        DynamicArray<Size_t> getProducedKeys() const override { return MakeKeyHashes(Produces{}); }
        DynamicArray<Size_t> getConsumedKeys() const override { return MakeKeyHashes(Consumes{}); }

        void build(RenderGraphBuilder& graph,
                   const RenderPassBuildContext& context) override;
    };

} // namespace dodoe
