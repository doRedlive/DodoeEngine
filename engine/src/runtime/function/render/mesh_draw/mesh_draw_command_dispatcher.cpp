// do@Redlive
#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "mesh_draw_command_dispatcher.h"
#include "mesh_draw_types.h"

namespace dodoe {

    void MeshDrawCommandDispatcher::UploadInstanceTransforms(
        const RenderGraphPassContext& context,
        const DynamicArray<InstanceSceneData>& instance_data,
        const RenderGraphBufferHandle& primitive_scene_data,
        DrawCommandList& command_list)
    {
        if (instance_data.empty() || !primitive_scene_data.isValid()) {
            return;
        }
        command_list.writeBuffer(
            context.resolveBuffer(primitive_scene_data),
            instance_data.data(),
            instance_data.size() * sizeof(InstanceSceneData)
        );
    }

    void MeshDrawCommandDispatcher::DispatchCached(
        const RenderGraphPassContext& context,
        const MeshPassType pass_type,
        const DynamicArray<GBufferMeshDrawShaderData>& gbuffer_shader_data,
        const DynamicArray<MeshDrawInstance>& instances,
        const DynamicArray<MeshDrawCommand>& commands,
        const GfxFramebufferHandle& framebuffer,
        const GfxViewportState& viewport_state,
        const GfxGraphicsPipelineHandle& pass_pipeline,
        const RenderGraphBufferHandle& primitive_scene_buffer,
        const RenderGraphBufferHandle& pass_constant_buffer,
        DrawCommandList& command_list)
    {
        if (instances.empty()) {
            return;
        }

        if (pass_constant_buffer.isValid() && pass_type == MeshPassType::GBuffer) {
            auto* resolved_buffer = context.resolveBuffer(pass_constant_buffer);
            command_list.setBufferState(resolved_buffer, GfxResourceStates::CopyDest);
            command_list.commitBarriers();

            for (const auto& instance : instances) {
                if (!instance.hasShaderData()) {
                    continue;
                }
                DO_ASSERT(
                    instance.shader_data_index < gbuffer_shader_data.size(),
                    "DispatchCached gbuffer shader data index out of range");
                command_list.writeBuffer(resolved_buffer,
                    &gbuffer_shader_data[instance.shader_data_index],
                    sizeof(gbuffer_shader_data[instance.shader_data_index]),
                    instance.shader_data_index * sizeof(gbuffer_shader_data[instance.shader_data_index]));
            }

            command_list.setBufferState(resolved_buffer, GfxResourceStates::ConstantBuffer);
            command_list.commitBarriers();
        }

        for (const auto& instance : instances) {
            const auto& cached_cmd = commands[instance.cmd_index];

            const auto pipeline = cached_cmd.pipeline ? cached_cmd.pipeline : pass_pipeline;
            if (!pipeline) {
                continue;
            }

            auto graphics_state = GfxGraphicsState()
                .setFramebuffer(framebuffer->getRHI())
                .setViewport(viewport_state)
                .setPipeline(pipeline->getRHIHandle());

            for (const auto& binding_set : cached_cmd.binding_sets) {
                if (binding_set && binding_set->isRHIReady()) {
                    graphics_state.addBindingSet(binding_set->getRHIHandle());
                }
            }

            for (const auto& vertex_binding : cached_cmd.vertex_bindings) {
                graphics_state.addVertexBuffer(vertex_binding);
            }
            if (primitive_scene_buffer.isValid()) {
                graphics_state.addVertexBuffer(
                    GfxVertexBufferBinding()
                        .setBuffer(context.resolveBuffer(primitive_scene_buffer)->getRHI())
                        .setSlot(1)
                        .setOffset(instance.instance_offset)
                );
            }

            graphics_state.setIndexBuffer(cached_cmd.index_binding);
            command_list.setGraphicsState(graphics_state);
            command_list.drawIndexed(cached_cmd.draw_args);
        }
    }

} // dodoe
