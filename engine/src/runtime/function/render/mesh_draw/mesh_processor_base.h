// do@Redlive

#pragma once

#include "dopch.h"

#include "mesh_draw_command.h"
#include "mesh_draw_types.h"
#include "mesh_pass_type.h"
#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"
#include "runtime/core/math/math.h"

namespace dodoe {

    class DrawCommandList;
    class PrimitiveSceneInfo;
    class MeshBatch;
    struct MeshBatchElement;
    struct MeshPassRelevance;
    class MeshDrawCommandCache;
    class BindingLayoutCache;
    class DescriptorTableManager;

    struct MeshPassPipelineContext {
        GfxShaderHandle vertex_shader{};
        GfxShaderHandle pixel_shader{};
        GfxInputLayoutHandle input_layout{};
        GfxBindingLayoutHandle pass_binding_layout{};
        DescriptorTableManager* descriptor_table{nullptr};
        BindingLayoutCache* binding_layout_cache{nullptr};
    };

    struct MeshPassCommandBuildContext {
        const DynamicArray<const PrimitiveSceneInfo*>& visible_primitives;
        const DynamicArray<MeshPassRelevance>& primitive_mesh_pass_relevance;
        const DynamicArray<UInt32>& mesh_pass_primitive_indices;
        const DynamicArray<UInt32>* primitive_instance_offsets{nullptr};
        const Matrix4f& view_projection;
        const Matrix4f& view_matrix;
        const GfxGraphicsPipelineHandle& pipeline;
        DynamicArray<MeshDrawCommandSource>& command_sources;
    };

    class MeshPassProcessor {
        MeshPassType m_pass_type;

    public:
        explicit MeshPassProcessor(MeshPassType pass_type) : m_pass_type(pass_type) {}
        virtual ~MeshPassProcessor() = default;
        virtual void reset() = 0;
        void buildMeshDrawCommands(const MeshPassCommandBuildContext& context) const;
        [[nodiscard]] virtual GfxGraphicsPipelineDesc buildPipelineDescription(
            const MeshPassPipelineContext&) const { return {}; }

        [[nodiscard]] MeshPassType getMeshPassType() const { return m_pass_type; }
        [[nodiscard]] virtual const GfxBindingLayoutHandle& getGlobalBindingLayout() const = 0;
        [[nodiscard]] virtual const GfxBindingLayoutHandle& getViewBindingLayout() const = 0;
        [[nodiscard]] virtual const GfxBindingLayoutHandle& getPrimitiveBindingLayout() const;
        [[nodiscard]] virtual const GfxBindingLayoutHandle& getSamplerBindingLayout() const;
        [[nodiscard]] virtual const GfxBufferHandle& getGlobalConstantBuffer() const = 0;
        [[nodiscard]] virtual const GfxBufferHandle& getViewConstantBuffer() const = 0;
        [[nodiscard]] virtual const GfxBufferHandle& getPrimitiveConstantBuffer() const;

    protected:
        [[nodiscard]] virtual Bool shouldDrawPrimitive(const PrimitiveSceneInfo& primitive) const = 0;
        [[nodiscard]] virtual Bool setupMeshDrawCommand(
            const MeshBatch& batch,
            const MeshBatchElement& element,
            MeshDrawCommand& command,
            PrimitiveMeshDrawShaderData& shader_data) const = 0;

        [[nodiscard]] static StaticArray<Vector4f, 6> ExtractFrustumPlanes(const Matrix4f& view_projection);
        [[nodiscard]] static Bool IntersectsFrustum(const StaticArray<Vector4f, 6>& planes,
                                                    const Vector3f& center, const Vector3f& extents);
        [[nodiscard]] static Bool IsBatchFrustumCulled(const MeshBatch& batch,
                                                       const PrimitiveSceneInfo* primitive,
                                                       const StaticArray<Vector4f, 6>& frustum_planes);

        [[nodiscard]] MeshDrawCommand BuildDrawCommand(const MeshBatchElement& element,
                                                       const GfxGraphicsPipelineHandle& pipeline) const;
    };

    void SubmitMeshDrawSources(
        const DynamicArray<MeshDrawCommandSource>& sources,
        const GfxBufferHandle& primitive_cb,
        const GfxFramebufferHandle& framebuffer,
        const GfxViewportState& viewport_state,
        const GfxBufferHandle& primitive_scene_buffer,
        const GfxBindingSetHandle* pass_binding_set,
        DrawCommandList& command_list);

} // namespace dodoe
