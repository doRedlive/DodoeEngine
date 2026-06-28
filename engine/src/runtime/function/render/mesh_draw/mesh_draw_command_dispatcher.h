// do@Redlive
#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#pragma once

#include "dopch.h"

#include "../render_graph/render_graph_pass.h"
#include "view_mesh_draw_context.h"

namespace dodoe {

    class MeshDrawCommandDispatcher {
    public:
        static void uploadInstanceTransforms(
            const RenderGraphPassContext& context,
            const ViewMeshInstanceData& instance_data,
            const RenderGraphBufferHandle& primitive_scene_data,
            DrawCommandList& command_list);

        static void dispatch(
            const RenderGraphPassContext& context,
            const MeshPassType pass_type,
            const ViewMeshShaderData& shader_data,
            const ViewMeshPassData& pass_data,
            const DynamicArray<MeshDrawCommand>& commands,
            const GfxFramebufferHandle& framebuffer,
            const GfxViewportState& viewport_state,
            const GfxGraphicsPipelineHandle& pass_pipeline,
            const RenderGraphBufferHandle& primitive_scene_buffer,
            const RenderGraphBufferHandle& pass_constant_buffer,
            DrawCommandList& command_list);
    };

} // dodoe
