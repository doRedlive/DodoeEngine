// do@Redlive

#pragma once

#include "dopch.h"

#include "vertex_factory.h"

namespace dodoe {

    class LocalVertexFactory final : public VertexFactory {
        GfxInputLayoutHandle m_gbuffer_input_layout{};
        GfxInputLayoutHandle m_shadow_input_layout{};

    public:
        void initialize(
            GfxContext& gfx_context,
            const GfxShaderHandle& gbuffer_vertex_shader,
            const GfxShaderHandle& shadow_vertex_shader);
        void reset();

        [[nodiscard]] const GfxInputLayoutHandle& getGBufferInputLayout() const { return m_gbuffer_input_layout; }
        [[nodiscard]] const GfxInputLayoutHandle& getShadowInputLayout() const { return m_shadow_input_layout; }
    };

} // dodoe
