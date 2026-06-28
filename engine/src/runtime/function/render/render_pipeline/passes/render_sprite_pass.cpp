// do@Redlive

#include "render_pipeline_passes.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "../../render_view/render_view.h"
#include "../render_pipeline_pass_utils.h"

#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/render_pipeline/render_feature/sprite_render_resources.h"
#include "runtime/function/render/framework/texture_manager.h"
#include "runtime/function/render/framework/descriptor_table_manager.h"
#include "runtime/core/utils/common.h"
#include "render_pass_blackboard_keys.h"

namespace dodoe::RenderPipelinePass {

    struct SpritePassParameters {
            RenderGraphTextureHandle color_target{};
            RenderGraphBufferHandle instance_buffer{};
            RenderGraphBufferHandle quad_vertex_buffer{};
            RenderGraphBufferHandle quad_index_buffer{};
            RenderGraphBufferHandle vp_buffer{};
        };

        void RenderSpritePass(RenderGraphBuilder& graph, const RenderPassContext& pass_context, SpriteRenderResources& resources) {
            graph.addPass<SpritePassParameters>(
                "MainSpritePass",
                RenderGraphPassFlags::Raster,
                [pass_context](RenderGraphPassBuilder& pass_builder, SpritePassParameters& parameters) {
                    const auto* scene_color = pass_builder.blackboard().get<FxaaColorKey, RenderGraphTextureHandle>();
                    if (scene_color) {
                        parameters.color_target = pass_builder.write(*scene_color);
                    } else {
                        const auto swapchain_extent = pass_context.gfx_context->getSwapchainExtent2d();
                        parameters.color_target = pass_builder.write(pass_builder.createTransientTexture(
                            rendering_pipeline_utils::MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA8_UNORM, "RDG SpriteColor"),
                            "SpriteColor"));
                    }

                    const auto* scene = pass_context.scene;
                    DO_ASSERT(scene != nullptr, "SpritePass scene is null");
                    const UInt32 instance_count = std::max(static_cast<UInt32>(scene->getSpriteSceneInfos().size()), 1u);

                    RenderGraphBufferDesc instance_buffer_desc{};
                    instance_buffer_desc.desc = GfxBufferDesc()
                        .setByteSize(instance_count * static_cast<UInt32>(sizeof(SpriteInstance)))
                        .setIsVertexBuffer(true)
                        .setDebugName("RDG SpriteInstanceBuffer");
                    parameters.instance_buffer = pass_builder.write(pass_builder.createTransientBuffer(instance_buffer_desc, "SpriteInstanceBuffer"));

                    RenderGraphBufferDesc quad_vb_desc{};
                    quad_vb_desc.desc = GfxBufferDesc()
                        .setByteSize(static_cast<UInt32>(sizeof(kQuadVertices)))
                        .setIsVertexBuffer(true)
                        .setDebugName("RDG SpriteQuadVB");
                    parameters.quad_vertex_buffer = pass_builder.write(pass_builder.createTransientBuffer(quad_vb_desc, "SpriteQuadVB"));

                    RenderGraphBufferDesc quad_ib_desc{};
                    quad_ib_desc.desc = GfxBufferDesc()
                        .setByteSize(static_cast<UInt32>(sizeof(kQuadIndices)))
                        .setIsIndexBuffer(true)
                        .setDebugName("RDG SpriteQuadIB");
                    parameters.quad_index_buffer = pass_builder.write(pass_builder.createTransientBuffer(quad_ib_desc, "SpriteQuadIB"));

                    RenderGraphBufferDesc vp_buf_desc{};
                    vp_buf_desc.desc = GfxBufferDesc().setByteSize(64).setIsConstantBuffer(true).setDebugName("SpriteVPBuffer");
                    parameters.vp_buffer = pass_builder.write(pass_builder.createTransientBuffer(vp_buf_desc, "SpriteVPBuffer"));
                },
                [pass_context, &resources](const SpritePassParameters& parameters, const RenderGraphPassContext& context, DrawCommandList& command_list) {
                    const auto* scene = pass_context.scene;
                    DO_ASSERT(scene != nullptr, "SpritePass scene is null");
                    const auto& sprite_infos = scene->getSpriteSceneInfos();

                    const auto color_target = context.resolveTexture(parameters.color_target);
                    const auto quad_vertex_buffer = context.resolveBuffer(parameters.quad_vertex_buffer);
                    const auto quad_index_buffer = context.resolveBuffer(parameters.quad_index_buffer);
                    const auto instance_buffer = context.resolveBuffer(parameters.instance_buffer);
                    const auto vp_buffer = context.resolveBuffer(parameters.vp_buffer);

                    command_list.setTextureState(color_target, GfxAllSubresources, GfxResourceStates::RenderTarget);
                    command_list.commitBarriers();

                    if (sprite_infos.empty()) {
                        return;
                    }

                    command_list.writeBuffer(quad_vertex_buffer, kQuadVertices, sizeof(kQuadVertices));
                    command_list.writeBuffer(quad_index_buffer, kQuadIndices, sizeof(kQuadIndices));
                    command_list.setBufferState(instance_buffer, GfxResourceStates::CopyDest);
                    command_list.commitBarriers();

                    DynamicArray<SpriteInstance> instances{};
                    instances.reserve(sprite_infos.size());
                    for (const auto& info : sprite_infos) {
                        instances.push_back(info.toInstance());
                    }
                    command_list.writeBuffer(instance_buffer, instances.data(), instances.size() * sizeof(SpriteInstance));

                    command_list.setBufferState(quad_vertex_buffer, GfxResourceStates::VertexBuffer);
                    command_list.setBufferState(quad_index_buffer, GfxResourceStates::IndexBuffer);
                    command_list.commitBarriers();

                    resources.renderSprites(pass_context, context, command_list, color_target, quad_vertex_buffer, quad_index_buffer, instance_buffer, vp_buffer);
                }
            );
        }

} // namespace dodoe::RenderPipelinePass
