// do@Redlive
#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "mesh_draw_command_dispatcher.h"
#include "mesh_draw_types.h"

namespace dodoe {
    namespace {

        void writePassShaderData(
            const RenderGraphPassContext& context,
            const MeshPassType pass_type,
            const DynamicArray<GBufferMeshDrawShaderData>& gbuffer_shader_data,
            const MeshDrawCommand& command,
            const RenderGraphBufferHandle& pass_constant_buffer,
            DrawCommandList& command_list)
        {
            if (!pass_constant_buffer.isValid() || pass_type != MeshPassType::GBuffer) {
                return;
            }

            DO_ASSERT(
                command.shader_data_index < gbuffer_shader_data.size(),
                "MeshDrawCommandDispatcher gbuffer shader data index out of range");
            command_list.setBufferState(context.resolveBuffer(pass_constant_buffer), GfxResourceStates::CopyDest);
            command_list.commitBarriers();
            command_list.writeBuffer(context.resolveBuffer(pass_constant_buffer), &gbuffer_shader_data[command.shader_data_index], sizeof(gbuffer_shader_data[command.shader_data_index]));
            command_list.setBufferState(context.resolveBuffer(pass_constant_buffer), GfxResourceStates::ConstantBuffer);
            command_list.commitBarriers();
        }

    } // namespace

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

    void MeshDrawCommandDispatcher::Dispatch(
        const RenderGraphPassContext& context,
        const MeshPassType pass_type,
        const DynamicArray<GBufferMeshDrawShaderData>& gbuffer_shader_data,
        const DynamicArray<MeshDrawCommand>& commands,
        const GfxFramebufferHandle& framebuffer,
        const GfxViewportState& viewport_state,
        const GfxGraphicsPipelineHandle& pass_pipeline,
        const RenderGraphBufferHandle& primitive_scene_buffer,
        const RenderGraphBufferHandle& pass_constant_buffer,
        DrawCommandList& command_list)
    {
        DO_DEBUG("MeshDrawCommandDispatcher: Pass type {}, command count: {}", static_cast<int>(pass_type), commands.size());

        if (commands.empty()) {
            return;
        }

        GfxGraphicsPipelineHandle current_pipeline = nullptr;
        for (const auto& command : commands) {
            if (!command.isValid()) {
                continue;
            }

            const auto pipeline = command.usesPassPipeline() ? pass_pipeline : command.pipeline;
            if (!pipeline) {
                continue;
            }

            writePassShaderData(context, pass_type, gbuffer_shader_data, command, pass_constant_buffer, command_list);

            auto graphics_state = GfxGraphicsState()
                .setFramebuffer(framebuffer->getRHI())
                .setViewport(viewport_state);

            if (pipeline != current_pipeline) {
                graphics_state.setPipeline(pipeline->getRHIHandle());
                current_pipeline = pipeline;
            }

            for (const auto& binding_set : command.binding_sets) {
                if (binding_set && binding_set->isRHIReady()) {
                    graphics_state.addBindingSet(binding_set->getRHIHandle());
                }
            }

            for (const auto& vertex_binding : command.vertex_bindings) {
                graphics_state.addVertexBuffer(vertex_binding);
            }
            if (command.uses_primitive_scene_buffer && primitive_scene_buffer.isValid()) {
                graphics_state.addVertexBuffer(
                    GfxVertexBufferBinding()
                        .setBuffer(context.resolveBuffer(primitive_scene_buffer)->getRHI())
                        .setSlot(command.primitive_scene_buffer_slot)
                        .setOffset(command.primitive_scene_buffer_offset)
                );
            }

            graphics_state.setIndexBuffer(command.index_binding);
            command_list.setGraphicsState(graphics_state);
            command_list.drawIndexed(command.draw_args);
        }
    }

} // dodoe
