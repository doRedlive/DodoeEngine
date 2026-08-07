// do@Redlive

#pragma once

#include "dopch.h"

#include "mesh_processor_base.h"
#include "cached_mesh_draw_command.h"
#include "../render_scene/primitive_scene_info.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

    class PrimitiveSceneInfo;
    class BindingLayoutCache;
    class BindingSetCache;
    struct MeshPassRelevance;

    class DirectionalShadowMeshProcessor final : public IMeshPassProcessor {
        GfxBindingLayoutHandle m_global_binding_layout{};
        GfxBindingLayoutHandle m_view_binding_layout{};
        GfxBindingSetHandle m_global_binding_set{};
        GfxBindingSetHandle m_view_binding_set{};
        GfxBufferHandle m_global_constant_buffer{};
        GfxBufferHandle m_view_constant_buffer{};

    public:
        DirectionalShadowMeshProcessor(BindingLayoutCache& binding_layout_cache,
                                       BindingSetCache& binding_set_cache);
        void reset() override;

        [[nodiscard]] const GfxBindingLayoutHandle& getGlobalBindingLayout() const { return m_global_binding_layout; }
        [[nodiscard]] const GfxBindingLayoutHandle& getViewBindingLayout() const { return m_view_binding_layout; }
        [[nodiscard]] const GfxBindingSetHandle& getGlobalBindingSet() const { return m_global_binding_set; }
        [[nodiscard]] const GfxBindingSetHandle& getViewBindingSet() const { return m_view_binding_set; }
        [[nodiscard]] const GfxBufferHandle& getGlobalConstantBuffer() const { return m_global_constant_buffer; }
        [[nodiscard]] const GfxBufferHandle& getViewConstantBuffer() const { return m_view_constant_buffer; }

        void buildCachedCommands(
            const DynamicArray<const PrimitiveSceneInfo*>& visible_primitives,
            const DynamicArray<MeshPassRelevance>& primitive_mesh_pass_relevance,
            const DynamicArray<UInt32>& mesh_pass_primitive_indices,
            const Matrix4f& light_view_projection,
            MeshDrawCommandCache& cache,
            DynamicArray<MeshDrawInstance>& out_instances) const;

        void buildDynamicCommands(
            const DynamicArray<const PrimitiveSceneInfo*>& visible_primitives,
            const DynamicArray<MeshPassRelevance>& primitive_mesh_pass_relevance,
            const DynamicArray<UInt32>& mesh_pass_primitive_indices,
            const Matrix4f& light_view_projection,
            DynamicArray<MeshDrawCommand>& frame_commands,
            DynamicArray<MeshDrawInstance>& out_instances) const;

    };

} // dodoe
