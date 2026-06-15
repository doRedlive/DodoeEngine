// do@Redlive

#pragma once

#include "dopch.h"

#include "../framework/primitive_scene_info.h"
#include "runtime/function/graphics/gfx_context.h"
#include "view_mesh_draw_context.h"

namespace dodoe {

    class DirectionalShadowMeshProcessor {
        GfxBindingLayoutHandle m_binding_layout{};
        GfxBindingSetHandle m_binding_set{};
        GfxBufferHandle m_constant_buffer{};

    public:
        void initialize(GfxContext& gfx_context);
        void reset();

        [[nodiscard]] const GfxBindingLayoutHandle& getBindingLayout() const { return m_binding_layout; }
        [[nodiscard]] const GfxBindingSetHandle& getBindingSet() const { return m_binding_set; }
        [[nodiscard]] const GfxBufferHandle& getConstantBuffer() const { return m_constant_buffer; }

        void buildCommands(
            const ViewMeshVisibilityData& visibility_data,
            const ViewMeshInstanceData& instance_data,
            const ViewMeshPassData& view_pass_data,
            const Matrix4f& light_view_projection,
            ViewMeshPassData& out_pass_data) const;
    };

} // dodoe
