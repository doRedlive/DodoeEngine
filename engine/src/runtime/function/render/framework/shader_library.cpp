// do@Redlive

#include "shader_library.h"
#include "runtime/core/utils/common.h"

namespace dodoe {

    void ShaderLibrary::initialize(GfxContext& gfx_context) {
        const auto device = gfx_context.getDevice();
        DO_ASSERT(device != nullptr, "ShaderLibrary device is null");

        auto load_shader = [device](const char* path, const GfxShaderType shader_type, const char* debug_name) {
            auto source = ReadShaderFile(path);
            if (source.empty()) {
                DO_ERROR("ShaderLibrary::load_shader failed to read shader file: {}", path);
                return GfxShaderHandle{};
            }
            auto shader = device->createShader(
                GfxShaderDesc().setShaderType(shader_type).setEntryName("main").setDebugName(debug_name),
                source.data(),
                source.size()
            );
            if (!shader) {
                DO_ERROR("ShaderLibrary::load_shader createShader failed for: {}", path);
            }
            return shader;
        };

        m_gbuffer_vertex_shader = load_shader("engine/res/shaders/bin/main_camera_pass.vert.spv", GfxShaderType::Vertex, "ShaderLibrary GBuffer VS");
        m_gbuffer_pixel_shader = load_shader("engine/res/shaders/bin/main_camera_pass.frag.spv", GfxShaderType::Pixel, "ShaderLibrary GBuffer PS");
        m_shadow_vertex_shader = load_shader("engine/res/shaders/bin/directional_light_shadow_pass.vert.spv", GfxShaderType::Vertex, "ShaderLibrary Shadow VS");
        m_shadow_pixel_shader = load_shader("engine/res/shaders/bin/directional_light_shadow_pass.frag.spv", GfxShaderType::Pixel, "ShaderLibrary Shadow PS");
        m_fullscreen_vertex_shader = load_shader("engine/res/shaders/bin/fullscreen.vert.spv", GfxShaderType::Vertex, "ShaderLibrary Fullscreen VS");
        m_skybox_pixel_shader = load_shader("engine/res/shaders/bin/skybox_pass.frag.spv", GfxShaderType::Pixel, "ShaderLibrary Skybox PS");
        m_deferred_light_pixel_shader = load_shader("engine/res/shaders/bin/deferred_light_pass.frag.spv", GfxShaderType::Pixel, "ShaderLibrary DeferredLight PS");
        m_tone_mapping_pixel_shader = load_shader("engine/res/shaders/bin/tone_mapping_pass.frag.spv", GfxShaderType::Pixel, "ShaderLibrary ToneMapping PS");
        m_color_grading_pixel_shader = load_shader("engine/res/shaders/bin/color_grading_pass.frag.spv", GfxShaderType::Pixel, "ShaderLibrary ColorGrading PS");
        m_fxaa_pixel_shader = load_shader("engine/res/shaders/bin/fxaa_pass.frag.spv", GfxShaderType::Pixel, "ShaderLibrary FXAA PS");
        m_present_pixel_shader = load_shader("engine/res/shaders/bin/combine_pass.frag.spv", GfxShaderType::Pixel, "ShaderLibrary Present PS");
        m_imgui_vertex_shader = load_shader("engine/res/shaders/bin/imgui_pass.vert.spv", GfxShaderType::Vertex, "ShaderLibrary ImGui VS");
        m_imgui_pixel_shader = load_shader("engine/res/shaders/bin/imgui_pass.frag.spv", GfxShaderType::Pixel, "ShaderLibrary ImGui PS");
        m_sprite_vertex_shader = load_shader("engine/res/shaders/bin/sprite_pass.vert.spv", GfxShaderType::Vertex, "ShaderLibrary Sprite VS");
        m_sprite_pixel_shader = load_shader("engine/res/shaders/bin/sprite_pass.frag.spv", GfxShaderType::Pixel, "ShaderLibrary Sprite PS");

        DO_INFO("ShaderLibrary::initialize completed");
    }

    void ShaderLibrary::reset() {
        m_sprite_pixel_shader = nullptr;
        m_sprite_vertex_shader = nullptr;
        m_imgui_pixel_shader = nullptr;
        m_imgui_vertex_shader = nullptr;
        m_present_pixel_shader = nullptr;
        m_fxaa_pixel_shader = nullptr;
        m_color_grading_pixel_shader = nullptr;
        m_tone_mapping_pixel_shader = nullptr;
        m_deferred_light_pixel_shader = nullptr;
        m_skybox_pixel_shader = nullptr;
        m_fullscreen_vertex_shader = nullptr;
        m_shadow_pixel_shader = nullptr;
        m_shadow_vertex_shader = nullptr;
        m_gbuffer_pixel_shader = nullptr;
        m_gbuffer_vertex_shader = nullptr;
    }

} // dodoe
