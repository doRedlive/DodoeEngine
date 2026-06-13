// do@Redlive

#pragma once

#include "dopch.h"

#include "../mesh_draw/mesh_pass_type.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    struct GraphicsPipelineCacheKey {
        MeshPassType pass_type{MeshPassType::GBuffer};
        PrimitiveType primitive_type{PrimitiveType::TriangleList};
        UInt32 patch_control_points{0};
        GfxInputLayout* input_layout{nullptr};
        GfxShader* vertex_shader{nullptr};
        GfxShader* hull_shader{nullptr};
        GfxShader* domain_shader{nullptr};
        GfxShader* geometry_shader{nullptr};
        GfxShader* pixel_shader{nullptr};
        DynamicArray<GfxBindingLayout*> binding_layouts{};
        RenderState render_state{};
        nvrhi::VariableRateShadingState shading_rate_state{};
        nvrhi::FramebufferInfo framebuffer_info{};

        [[nodiscard]] Bool operator==(const GraphicsPipelineCacheKey& other) const;
    };

    struct GraphicsPipelineCacheKeyHash {
        [[nodiscard]] Size_t operator()(const GraphicsPipelineCacheKey& key) const;
    };

} // dodoe
