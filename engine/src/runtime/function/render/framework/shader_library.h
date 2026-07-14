// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/render/render_settings.h"

namespace dodoe {

    class ShaderLibrary {
        GfxShaderHandle m_gbuffer_vertex_shader{};
        GfxShaderHandle m_gbuffer_pixel_shader{};
        GfxShaderHandle m_shadow_vertex_shader{};
        GfxShaderHandle m_shadow_pixel_shader{};
        GfxShaderHandle m_fullscreen_vertex_shader{};
        GfxShaderHandle m_skybox_pixel_shader{};
        GfxShaderHandle m_deferred_light_pixel_shader{};
        GfxShaderHandle m_tone_mapping_pixel_shader{};
        GfxShaderHandle m_color_grading_pixel_shader{};
        GfxShaderHandle m_fxaa_pixel_shader{};
        GfxShaderHandle m_present_pixel_shader{};
        GfxShaderHandle m_imgui_vertex_shader{};
        GfxShaderHandle m_imgui_pixel_shader{};
        GfxShaderHandle m_sprite_vertex_shader{};
        GfxShaderHandle m_sprite_pixel_shader{};
        GfxShaderHandle m_sprite_pixel_shader_traditional{};
        GfxShaderHandle m_test_vertex_shader{};
        GfxShaderHandle m_test_pixel_shader{};
        GfxShaderHandle m_gpu_culling_compute_shader{};

    public:
        void initialize(GfxContext& gfx_context);
        void reset();

        [[nodiscard]] const GfxShaderHandle& getGBufferVertexShader() const { return m_gbuffer_vertex_shader; }
        [[nodiscard]] const GfxShaderHandle& getGBufferPixelShader() const { return m_gbuffer_pixel_shader; }
        [[nodiscard]] const GfxShaderHandle& getShadowVertexShader() const { return m_shadow_vertex_shader; }
        [[nodiscard]] const GfxShaderHandle& getShadowPixelShader() const { return m_shadow_pixel_shader; }
        [[nodiscard]] const GfxShaderHandle& getFullscreenVertexShader() const { return m_fullscreen_vertex_shader; }
        [[nodiscard]] const GfxShaderHandle& getSkyboxPixelShader() const { return m_skybox_pixel_shader; }
        [[nodiscard]] const GfxShaderHandle& getDeferredLightPixelShader() const { return m_deferred_light_pixel_shader; }
        [[nodiscard]] const GfxShaderHandle& getToneMappingPixelShader() const { return m_tone_mapping_pixel_shader; }
        [[nodiscard]] const GfxShaderHandle& getColorGradingPixelShader() const { return m_color_grading_pixel_shader; }
        [[nodiscard]] const GfxShaderHandle& getFxaaPixelShader() const { return m_fxaa_pixel_shader; }
        [[nodiscard]] const GfxShaderHandle& getPresentPixelShader() const { return m_present_pixel_shader; }
        [[nodiscard]] const GfxShaderHandle& getImGuiVertexShader() const { return m_imgui_vertex_shader; }
        [[nodiscard]] const GfxShaderHandle& getImGuiPixelShader() const { return m_imgui_pixel_shader; }
        [[nodiscard]] const GfxShaderHandle& getSpriteVertexShader() const { return m_sprite_vertex_shader; }
        [[nodiscard]] const GfxShaderHandle& getSpritePixelShader() const { return m_sprite_pixel_shader; }
        [[nodiscard]] const GfxShaderHandle& getSpritePixelShaderTraditional() const { return m_sprite_pixel_shader_traditional; }
        [[nodiscard]] const GfxShaderHandle& getTestVertexShader() const { return m_test_vertex_shader; }
        [[nodiscard]] const GfxShaderHandle& getTestPixelShader() const { return m_test_pixel_shader; }
        [[nodiscard]] const GfxShaderHandle& getGpuCullingComputeShader() const { return m_gpu_culling_compute_shader; }
    };

} // dodoe
