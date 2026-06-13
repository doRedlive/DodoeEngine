// do@Redlive

#pragma once

#include "dopch.h"

#include "../render_graph/render_graph_pass.h"
#include "view_mesh_draw_context.h"

namespace dodoe {

    class MeshDrawCommandDispatcher {
    public:
        static void uploadInstanceTransforms(
            const ViewMeshDrawContext& view_context,
            const RenderGraphBufferHandle& primitive_scene_data,
            RenderGraphCommandList& command_list);

        static void dispatch(
            const MeshPassType pass_type,
            const ViewMeshDrawContext& view_context,
            const DynamicArray<MeshDrawCommand>& commands,
            const FramebufferHandle& framebuffer,
            const ViewportState& viewport_state,
            const GraphicsPipelineHandle& pass_pipeline,
            const GfxBufferHandle& primitive_scene_buffer,
            const GfxBufferHandle& pass_constant_buffer,
            RenderGraphCommandList& command_list);
    };

} // dodoe
