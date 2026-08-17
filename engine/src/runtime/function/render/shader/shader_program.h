// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/render/shader/shader_reflection.h"

namespace dodoe {

    enum class ShaderDomain : UInt8 {
        Global,
        Material,
    };

    struct ShaderProgram {
        String name{};
        GfxShaderHandle vertex_shader{};
        GfxShaderHandle hull_shader{};
        GfxShaderHandle domain_shader{};
        GfxShaderHandle geometry_shader{};
        GfxShaderHandle pixel_shader{};
        GfxShaderHandle compute_shader{};
        const ShaderReflectionData* vertex_reflection{nullptr};
        const ShaderReflectionData* hull_reflection{nullptr};
        const ShaderReflectionData* domain_reflection{nullptr};
        const ShaderReflectionData* geometry_reflection{nullptr};
        const ShaderReflectionData* pixel_reflection{nullptr};
        const ShaderReflectionData* compute_reflection{nullptr};

        [[nodiscard]] Bool isGraphics() const { return vertex_shader && pixel_shader; }
        [[nodiscard]] Bool isCompute() const { return compute_shader != nullptr; }
    };

} // namespace dodoe
