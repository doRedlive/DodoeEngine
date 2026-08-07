// do@Redlive

#pragma once

#include "dopch.h"

#include "render_feature.h"
#include "runtime/function/render/render_pipeline/passes/render_ui_pass.h"

namespace dodoe {

    class UIFeature final : public IRenderFeature {
        GfxBindingLayoutHandle m_view_binding_layout{};
        GfxBindingLayoutHandle m_bindless_binding_layout{};
        GfxBindingLayoutHandle m_material_binding_layout{};
        GfxInputLayoutHandle m_input_layout{};

    public:
        void initialize(SharedRenderService& resources) override;
        void shutdown() override;

        void collectPasses(PassCollector& collector) override;

        [[nodiscard]] GfxBindingLayoutHandle getViewBindingLayout() const { return m_view_binding_layout; }
        [[nodiscard]] GfxBindingLayoutHandle getBindlessBindingLayout() const { return m_bindless_binding_layout; }
        [[nodiscard]] GfxBindingLayoutHandle getMaterialBindingLayout() const { return m_material_binding_layout; }
        [[nodiscard]] GfxInputLayoutHandle getInputLayout() const { return m_input_layout; }
    };

} // namespace dodoe
