#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

    class FullscreenPassSharedState {
        GfxSamplerHandle m_screen_sampler{};
        GfxBindingLayoutHandle m_single_input_binding_layout{};
        GfxBindingLayoutHandle m_skybox_binding_layout{};
        GfxBindingLayoutHandle m_deferred_light_binding_layout{};
        GfxBindingLayoutHandle m_color_grading_binding_layout{};
        GfxBindingLayoutHandle m_present_binding_layout{};
        GfxBufferHandle m_deferred_light_constant_buffer{};

    public:
        FullscreenPassSharedState() = default;
        ~FullscreenPassSharedState() = default;

        Bool initialize(GfxContext& gfx_context);
        void reset();

        [[nodiscard]] const GfxSamplerHandle& getScreenSampler() const { return m_screen_sampler; }
        [[nodiscard]] const GfxBindingLayoutHandle& getSingleInputBindingLayout() const { return m_single_input_binding_layout; }
        [[nodiscard]] const GfxBindingLayoutHandle& getSkyboxBindingLayout() const { return m_skybox_binding_layout; }
        [[nodiscard]] const GfxBindingLayoutHandle& getDeferredLightBindingLayout() const { return m_deferred_light_binding_layout; }
        [[nodiscard]] const GfxBindingLayoutHandle& getColorGradingBindingLayout() const { return m_color_grading_binding_layout; }
        [[nodiscard]] const GfxBindingLayoutHandle& getPresentBindingLayout() const { return m_present_binding_layout; }
        [[nodiscard]] const GfxBufferHandle& getDeferredLightConstantBuffer() const { return m_deferred_light_constant_buffer; }
    };

} // dodoe
