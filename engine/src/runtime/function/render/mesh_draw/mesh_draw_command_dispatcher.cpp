// do@Redlive
#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "mesh_draw_command_dispatcher.h"

namespace dodoe {
    namespace {

        void writePassShaderData(
            const MeshPassType pass_type,
            const DynamicArray<GBufferMeshDrawShaderData>& gbuffer_shader_data,
            const MeshDrawCommand& command,
            const RenderGraphBufferHandle& pass_constant_buffer,
            RenderGraphCommandList& command_list)
        {
            if (!pass_constant_buffer.isValid() || pass_type != MeshPassType::GBuffer) {
                return;
            }

            DO_ASSERT(
                command.shader_data_index < gbuffer_shader_data.size(),
                "MeshDrawCommandDispatcher gbuffer shader data index out of range");
            command_list.setBufferState(pass_constant_buffer, GfxResourceStates::CopyDest);
            command_list.commitBarriers();
            command_list.writeBuffer(pass_constant_buffer, &gbuffer_shader_data[command.shader_data_index], sizeof(gbuffer_shader_data[command.shader_data_index]));
            command_list.setBufferState(pass_constant_buffer, GfxResourceStates::ConstantBuffer);
            command_list.commitBarriers();
        }

    } // namespace

    void MeshDrawCommandDispatcher::uploadInstanceTransforms(
        const ViewMeshInstanceData& instance_data,
        const RenderGraphBufferHandle& primitive_scene_data,
        RenderGraphCommandList& command_list)
    {
        for (Size_t primitive_index = 0; primitive_index < instance_data.instance_scene_data.size(); primitive_index++) {
            command_list.writeBuffer(
                primitive_scene_data,
                &instance_data.instance_scene_data[primitive_index],
                sizeof(InstanceSceneData),
                primitive_index * sizeof(InstanceSceneData)
            );
        }
    }

    void MeshDrawCommandDispatcher::dispatch(
        const MeshPassType pass_type,
        const ViewMeshShaderData& shader_data,
        const ViewMeshPassData& pass_data,
        const DynamicArray<MeshDrawCommand>& commands,
        const GfxFramebufferHandle& framebuffer,
        const GfxViewportState& viewport_state,
        const GfxGraphicsPipelineHandle& pass_pipeline,
        const RenderGraphBufferHandle& primitive_scene_buffer,
        const RenderGraphBufferHandle& pass_constant_buffer,
        RenderGraphCommandList& command_list)
    {
        if (commands.empty()) {
            return;
        }

        if (pass_type == MeshPassType::DirectionalShadow && pass_constant_buffer.isValid()) {
            struct DirectionalShadowPassShaderData {
                Matrix4f light_view_projection{1.0f};
                Vector4f time_data{0.0f};
            };
            DirectionalShadowPassShaderData directional_shader_data{};
            directional_shader_data.light_view_projection = shader_data.directional_shadow_view_projection;
            directional_shader_data.time_data = shader_data.frame_time_data;
            command_list.setBufferState(pass_constant_buffer, GfxResourceStates::CopyDest);
            command_list.commitBarriers();
            command_list.writeBuffer(pass_constant_buffer, &directional_shader_data, sizeof(directional_shader_data));
            command_list.setBufferState(pass_constant_buffer, GfxResourceStates::ConstantBuffer);
            command_list.commitBarriers();
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

            writePassShaderData(pass_type, shader_data.gbuffer_shader_data, command, pass_constant_buffer, command_list);

            auto graphics_state = GfxGraphicsState()
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
            if (command.uses_primitive_scene_buffer && primitive_scene_buffer.isValid()) {
                graphics_state.addVertexBuffer(
                    GfxVertexBufferBinding()
                        .setBuffer(command_list.resolveBuffer(primitive_scene_buffer))
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
