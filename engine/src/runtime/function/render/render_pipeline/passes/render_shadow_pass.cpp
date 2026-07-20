// do@Redlive

#include "render_pipeline_passes.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "render_pass_blackboard_keys.h"

#include "../../render_view/render_view.h"
#include "../../render_view/mesh_view_extension.h"
#include "../render_pipeline_pass_utils.h"

#include "runtime/function/render/mesh_draw/directional_shadow_mesh_processor.h"
#include "runtime/function/render/mesh_draw/mesh_draw_command_dispatcher.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"

namespace dodoe::RenderPipelinePass {

    struct ShadowPassParameters {
            RenderGraphTextureHandle shadow_map{};
            RenderGraphBufferHandle primitive_scene_buffer{};
            RenderGraphBufferHandle constant_buffer{};
        };

        void RenderDirectionalShadowPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context) {
            DO_ASSERT(pass_context.isValid(), "RenderingPipeline pass context is invalid");
            const auto& directional_shadow_mesh_processor = pass_context.getMeshProcessor<MeshPassType::DirectionalShadow>();

            graph.addPass<ShadowPassParameters>(
                "DirectionalShadowPass",
                RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
                [pass_context, &directional_shadow_mesh_processor](RenderGraphPassBuilder& pass_builder, ShadowPassParameters& parameters) {
                    const auto swapchain_extent = pass_context.gfx_context->getSwapchainExtent2d();
                    const auto* scene_textures = pass_builder.blackboard().get<SceneTexturesKey, SceneTextures>();
                    DO_ASSERT(scene_textures, "DirectionalShadowPass scene textures are missing");

                    parameters.shadow_map = pass_builder.write(pass_builder.createTransientTexture(
                        rendering_pipeline_utils::MakeSwapchainDepth2D(swapchain_extent, GfxFormat::D32, "RDG ShadowMap"),
                        "ShadowMap"));
                    parameters.primitive_scene_buffer = pass_builder.read(scene_textures->instance_scene_data);
                    parameters.constant_buffer = pass_builder.importBuffer(directional_shadow_mesh_processor.getConstantBuffer(), "DirectionalShadowConstantBuffer");
                    pass_builder.blackboard().set<ShadowMapKey>(parameters.shadow_map);
                },
                [&directional_shadow_mesh_processor](const ShadowPassParameters& parameters, const RenderGraphPassContext& context, DrawCommandList& command_list) {
                    DO_ASSERT(context.getView() != nullptr, "DirectionalShadowPass view is null");
                    const auto* view = context.getView();

                    const auto* mesh_ext = view->getExtension<MeshViewExtension>();
                    if (!mesh_ext) {
                        return;
                    }

                    const auto shadow_map = context.resolveTexture(parameters.shadow_map);

                    auto framebuffer_desc = GfxFramebufferDesc().setDepthAttachment(shadow_map);
                    auto framebuffer = command_list.createFramebuffer( framebuffer_desc);

                    const auto viewport_state = rendering_pipeline_utils::BuildViewportState(*context.getView(), context.getGfxContext()->getSwapchainExtent2d());

                    command_list.setTextureState(shadow_map, GfxAllSubresources, GfxResourceStates::DepthWrite);
                    command_list.commitBarriers();
                    command_list.clearDepthStencilTexture(shadow_map, GfxAllSubresources, true, 1.0f, false, 0);

                    const auto shadow_cb = context.resolveBuffer(parameters.constant_buffer);
                    struct ShadowConstantData {
                        Matrix4f light_view_projection;
                        Vector4f time_data;
                    };
                    const ShadowConstantData shadow_data{
                        mesh_ext->directional_shadow_view_projection,
                        mesh_ext->frame_time_data
                    };
                    command_list.setBufferState(shadow_cb, GfxResourceStates::CopyDest);
                    command_list.commitBarriers();
                    command_list.writeBuffer(shadow_cb, &shadow_data, sizeof(shadow_data));
                    command_list.setBufferState(shadow_cb, GfxResourceStates::ConstantBuffer);
                    command_list.commitBarriers();

                    MeshDrawCommandDispatcher::DispatchCached(
                        context,
                        MeshPassType::DirectionalShadow,
                        mesh_ext->gbuffer_shader_data,
                        mesh_ext->cached_draw_instances[static_cast<size_t>(MeshPassType::DirectionalShadow)],
                        *mesh_ext->cached_commands,
                        framebuffer,
                        viewport_state,
                        GfxGraphicsPipelineHandle{},
                        parameters.primitive_scene_buffer,
                        parameters.constant_buffer,
                        command_list
                    );
                    MeshDrawCommandDispatcher::DispatchCached(
                        context,
                        MeshPassType::DirectionalShadow,
                        mesh_ext->dynamic_shader_data,
                        mesh_ext->dynamic_draw_instances[static_cast<size_t>(MeshPassType::DirectionalShadow)],
                        mesh_ext->frame_commands,
                        framebuffer,
                        viewport_state,
                        GfxGraphicsPipelineHandle{},
                        parameters.primitive_scene_buffer,
                        parameters.constant_buffer,
                        command_list
                    );
                    command_list.setTextureState(shadow_map, GfxAllSubresources, GfxResourceStates::ShaderResource);
                    command_list.commitBarriers();
                }
            );
        }

} // namespace dodoe::RenderPipelinePass
