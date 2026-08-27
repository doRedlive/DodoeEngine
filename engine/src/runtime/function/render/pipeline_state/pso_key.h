// do@Redlive

#pragma once

#include "dopch.h"

#include "../mesh_draw/mesh_pass_type.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    struct GraphicsPipelineCacheKey {
        MeshPassType pass_type{MeshPassType::Opaque};
        GfxPrimitiveType primitive_type{GfxPrimitiveType::TriangleList};
        UInt32 patch_control_points{0};
        GfxInputLayout* input_layout{nullptr};
        GfxShader* vertex_shader{nullptr};
        GfxShader* hull_shader{nullptr};
        GfxShader* domain_shader{nullptr};
        GfxShader* geometry_shader{nullptr};
        GfxShader* pixel_shader{nullptr};
        DynamicArray<GfxBindingLayout*> binding_layouts{};
        GfxRenderState render_state{};
        GfxVariableRateShadingState shading_rate_state{};
        GfxFramebufferInfo framebuffer_info{};

        [[nodiscard]] Bool operator==(const GraphicsPipelineCacheKey& other) const;
    };

    struct GraphicsPipelineCacheKeyHash {
        [[nodiscard]] Size_t operator()(const GraphicsPipelineCacheKey& key) const;
    };

} // dodoe
