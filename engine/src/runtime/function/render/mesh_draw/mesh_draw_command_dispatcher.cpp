// do@Redlive

#include "mesh_draw_command_dispatcher.h"

namespace dodoe {
    namespace {

        void writePassShaderData(
            const MeshPassType pass_type,
            const ViewMeshDrawContext& view_context,
            const MeshDrawCommand& command,
            const GfxBufferHandle& pass_constant_buffer,
            RenderGraphCommandList& command_list)
        {
            if (!pass_constant_buffer) {
                return;
            }

            if (pass_type == MeshPassType::GBuffer) {
                DO_ASSERT(
                    command.shader_data_index < view_context.gbuffer_shader_data.size(),
                    "MeshDrawCommandDispatcher gbuffer shader data index out of range");
                command_list.writeBuffer(pass_constant_buffer, view_context.gbuffer_shader_data[command.shader_data_index]);
                return;
            }

        }

    } // namespace

    void MeshDrawCommandDispatcher::uploadInstanceTransforms(
        const ViewMeshDrawContext& view_context,
        const RenderGraphBufferHandle& primitive_scene_data,
        RenderGraphCommandList& command_list)
    {
        for (Size_t primitive_index = 0; primitive_index < view_context.instance_scene_data.size(); primitive_index++) {
            command_list.writeBuffer(
                primitive_scene_data,
                view_context.instance_scene_data[primitive_index],
                primitive_index * sizeof(InstanceSceneData)
            );
        }
    }

    void MeshDrawCommandDispatcher::dispatch(
        const MeshPassType pass_type,
        const ViewMeshDrawContext& view_context,
        const DynamicArray<MeshDrawCommand>& commands,
        const FramebufferHandle& framebuffer,
        const ViewportState& viewport_state,
        const GraphicsPipelineHandle& pass_pipeline,
        const GfxBufferHandle& primitive_scene_buffer,
        const GfxBufferHandle& pass_constant_buffer,
        RenderGraphCommandList& command_list)
    {
        if (commands.empty()) {
            return;
        }

        if (pass_type == MeshPassType::DirectionalShadow && pass_constant_buffer) {
            struct DirectionalShadowPassShaderData {
                Matrix4f light_view_projection{1.0f};
                Vector4f time_data{0.0f};
            };
            DirectionalShadowPassShaderData shader_data{};
            shader_data.light_view_projection = view_context.directional_shadow_view_projection;
            shader_data.time_data = view_context.frame_time_data;
            command_list.writeBuffer(pass_constant_buffer, shader_data);
        }

        GraphicsPipelineHandle current_pipeline = nullptr;
        for (const auto& command : commands) {
            if (!command.isValid()) {
                continue;
            }

            const auto pipeline = command.usesPassPipeline() ? pass_pipeline : command.pipeline;
            if (!pipeline) {
                continue;
            }

            writePassShaderData(pass_type, view_context, command, pass_constant_buffer, command_list);

            auto graphics_state = GraphicsState()
                .setFramebuffer(framebuffer)
                .setViewport(viewport_state);

            if (pipeline != current_pipeline) {
                graphics_state.setPipeline(pipeline);
                current_pipeline = pipeline;
            }

            for (const auto& binding_set : command.binding_sets) {
                if (binding_set) {
                    graphics_state.addBindingSet(binding_set);
                }
            }

            for (const auto& vertex_binding : command.vertex_bindings) {
                graphics_state.addVertexBuffer(vertex_binding);
            }
            if (command.uses_primitive_scene_buffer && primitive_scene_buffer) {
                graphics_state.addVertexBuffer(
                    VertexBufferBinding()
                        .setBuffer(primitive_scene_buffer)
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
