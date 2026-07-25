// do@Redlive

#pragma once

#include "dopch.h"

#include "mesh_processor_base.h"
#include "cached_mesh_draw_command.h"

#include "../render_scene/primitive_scene_info.h"
#include "runtime/function/graphics/gfx_context.h"

namespace dodoe {

    class PrimitiveSceneInfo;
    struct InstanceSceneData;
    struct MeshPassRelevance;
    struct GBufferMeshDrawShaderData;

    class GBufferMeshProcessor final : public IMeshPassProcessor {
        GfxBindingSetHandle m_descriptor_binding_set{};
        GfxSamplerHandle m_sampler{};
        GfxBindingLayoutHandle m_binding_layout{};
        GfxBindingSetHandle m_binding_set{};
        GfxBufferHandle m_constant_buffer{};

    public:
        GBufferMeshProcessor(GfxBindingSetHandle descriptor_binding_set = {});
        void reset() override;

        [[nodiscard]] const GfxBindingLayoutHandle& getBindingLayout() const override { return m_binding_layout; }
        [[nodiscard]] const GfxBindingSetHandle& getBindingSet() const { return m_binding_set; }
        [[nodiscard]] const GfxBufferHandle& getConstantBuffer() const override { return m_constant_buffer; }
        [[nodiscard]] const GfxBindingSetHandle& getDescriptorBindingSet() const { return m_descriptor_binding_set; }

        void buildCachedCommands(
            const DynamicArray<const PrimitiveSceneInfo*>& visible_primitives,
            const DynamicArray<MeshPassRelevance>& primitive_mesh_pass_relevance,
            const DynamicArray<UInt32>& mesh_pass_primitive_indices,
            const Matrix4f& view_projection,
            MeshDrawCommandCache& cache,
            DynamicArray<MeshDrawInstance>& out_instances,
            DynamicArray<GBufferMeshDrawShaderData>& out_shader_data) const;

        void buildDynamicCommands(
            const DynamicArray<const PrimitiveSceneInfo*>& visible_primitives,
            const DynamicArray<MeshPassRelevance>& primitive_mesh_pass_relevance,
            const DynamicArray<UInt32>& mesh_pass_primitive_indices,
            const Matrix4f& view_projection,
            DynamicArray<MeshDrawCommand>& frame_commands,
            DynamicArray<MeshDrawInstance>& out_instances,
            DynamicArray<GBufferMeshDrawShaderData>& out_shader_data) const;
    };

} // namespace dodoe
