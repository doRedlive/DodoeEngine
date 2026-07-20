// do@Redlive

#pragma once

#include "dopch.h"

#include "vertex_factory.h"
#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

    class LocalVertexFactory final : public VertexFactory {
        GfxInputLayoutHandle m_gbuffer_input_layout{};
        GfxInputLayoutHandle m_shadow_input_layout{};
        GfxInputLayoutHandle m_sprite_input_layout{};
        GfxInputLayoutHandle m_imgui_input_layout{};

    public:
        void initialize(
            DrawCommandList& command_list,
            const GfxShaderHandle& gbuffer_vertex_shader,
            const GfxShaderHandle& shadow_vertex_shader
        );
        void reset();

        [[nodiscard]] const GfxInputLayoutHandle& getGBufferInputLayout() const { return m_gbuffer_input_layout; }
        [[nodiscard]] const GfxInputLayoutHandle& getShadowInputLayout() const { return m_shadow_input_layout; }

        [[nodiscard]] GfxInputLayoutHandle getOrCreateSpriteInputLayout(
            DrawCommandList& command_list,
            GfxShaderHandle sprite_vs);

        [[nodiscard]] GfxInputLayoutHandle getOrCreateImGuiInputLayout(
            DrawCommandList& command_list,
            GfxShaderHandle imgui_vs);
    };

} // dodoe
