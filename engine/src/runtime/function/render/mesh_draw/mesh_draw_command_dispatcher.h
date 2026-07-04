// do@Redlive
#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#pragma once

#include "dopch.h"

#include "../render_graph/render_graph_pass.h"
#include "mesh_draw_command.h"

namespace dodoe {

    struct InstanceSceneData;
    struct GBufferMeshDrawShaderData;

    class MeshDrawCommandDispatcher {
    public:
        static void UploadInstanceTransforms(
            const RenderGraphPassContext& context,
            const DynamicArray<InstanceSceneData>& instance_data,
            const RenderGraphBufferHandle& primitive_scene_data,
            DrawCommandList& command_list);

        static void Dispatch(
            const RenderGraphPassContext& context,
            const MeshPassType pass_type,
            const DynamicArray<GBufferMeshDrawShaderData>& gbuffer_shader_data,
            const DynamicArray<MeshDrawCommand>& commands,
            const GfxFramebufferHandle& framebuffer,
            const GfxViewportState& viewport_state,
            const GfxGraphicsPipelineHandle& pass_pipeline,
            const RenderGraphBufferHandle& primitive_scene_buffer,
            const RenderGraphBufferHandle& pass_constant_buffer,
            DrawCommandList& command_list);
    };

} // dodoe
