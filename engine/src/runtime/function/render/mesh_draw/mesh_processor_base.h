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
    struct MeshBatch;
    struct MeshBatchElement;

    class IMeshPassProcessor {
    public:
        virtual ~IMeshPassProcessor() = default;
        virtual void reset() = 0;

    protected:
        [[nodiscard]] static StaticArray<Vector4f, 6> ExtractFrustumPlanes(const Matrix4f& view_projection);
        [[nodiscard]] static Bool IntersectsFrustum(const StaticArray<Vector4f, 6>& planes,
                                                    const Vector3f& center, const Vector3f& extents);
        [[nodiscard]] static Bool IsBatchFrustumCulled(const MeshBatch& batch,
                                                       const PrimitiveSceneInfo* primitive,
                                                       const StaticArray<Vector4f, 6>& frustum_planes);

        [[nodiscard]] static MeshDrawCommand BuildDrawCommand(const MeshBatchElement& element,
                                                              MeshPassType pass_type,
                                                              const GfxBindingSetHandle& primitive_binding_set);
    };

    void SubmitMeshDrawCommands(
        const DynamicArray<MeshDrawInstance>& instances,
        const DynamicArray<MeshDrawCommand>& commands,
        const DynamicArray<PrimitiveMeshDrawShaderData>& shader_data,
        const GfxBufferHandle& primitive_cb,
        const GfxFramebufferHandle& framebuffer,
        const GfxViewportState& viewport_state,
        const GfxBufferHandle& primitive_scene_buffer,
        DrawCommandList& command_list);

} // namespace dodoe
