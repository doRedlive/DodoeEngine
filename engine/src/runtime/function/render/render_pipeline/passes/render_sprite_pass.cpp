// do@Redlive

#include "render_pipeline_passes.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "../../render_view/render_view.h"
#include "../../render_view/sprite_view_extension.h"
#include "../render_pipeline_pass_utils.h"

#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/render_pipeline/render_feature/sprite_render_resources.h"
#include "runtime/function/render/framework/texture_manager.h"
#include "runtime/function/render/framework/descriptor_table_manager.h"
#include "runtime/core/utils/common.h"
#include "runtime/core/math/math.h"
#include "render_pass_blackboard_keys.h"

namespace dodoe::RenderPipelinePass {

    struct SpritePassParameters {
        RenderGraphTextureHandle color_target{};
        RenderGraphBufferHandle instance_buffer{};
        RenderGraphBufferHandle quad_vertex_buffer{};
        RenderGraphBufferHandle quad_index_buffer{};
        RenderGraphBufferHandle vp_buffer{};
    };

    void RenderSpritePass(RenderGraphBuilder& graph, const RenderView& view, const RenderPassContext& pass_context, SpriteRenderResources& resources) {
        graph.addPass<SpritePassParameters>(
            "MainSpritePass",
            RenderGraphPassFlags::Raster,
            [pass_context, &view](RenderGraphPassBuilder& pass_builder, SpritePassParameters& parameters) {
                const auto* scene_color = pass_builder.blackboard().get<FxaaColorKey, RenderGraphTextureHandle>();
                if (scene_color) {
                    parameters.color_target = pass_builder.write(*scene_color);
                } else {
                    const auto swapchain_extent = pass_context.gfx_context->getSwapchainExtent2d();
                    parameters.color_target = pass_builder.write(pass_builder.createTransientTexture(
                        rendering_pipeline_utils::MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA8_UNORM, "RDG SpriteColor"),
                        "SpriteColor"));
                }

                const auto* sprite_ext = view.getExtension<SpriteViewExtension>();
                const UInt32 visible_count = sprite_ext ? static_cast<UInt32>(sprite_ext->visible_sprites.size()) : 0;
                const UInt32 instance_count = std::max(visible_count, 1u);

                RenderGraphBufferDesc instance_buffer_desc{};
                instance_buffer_desc.desc = GfxBufferDesc()
                    .setByteSize(instance_count * static_cast<UInt32>(sizeof(SpriteInstance)))
                    .setIsVertexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
                    .setDebugName("RDG SpriteInstanceBuffer");
                parameters.instance_buffer = pass_builder.write(pass_builder.createTransientBuffer(instance_buffer_desc, "SpriteInstanceBuffer"));

                RenderGraphBufferDesc quad_vb_desc{};
                quad_vb_desc.desc = GfxBufferDesc()
                    .setByteSize(static_cast<UInt32>(sizeof(kQuadVertices)))
                    .setIsVertexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
                    .setDebugName("RDG SpriteQuadVB");
                parameters.quad_vertex_buffer = pass_builder.write(pass_builder.createTransientBuffer(quad_vb_desc, "SpriteQuadVB"));

                RenderGraphBufferDesc quad_ib_desc{};
                quad_ib_desc.desc = GfxBufferDesc()
                    .setByteSize(static_cast<UInt32>(sizeof(kQuadIndices)))
                    .setIsIndexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
                    .setDebugName("RDG SpriteQuadIB");
                parameters.quad_index_buffer = pass_builder.write(pass_builder.createTransientBuffer(quad_ib_desc, "SpriteQuadIB"));

                RenderGraphBufferDesc vp_buf_desc{};
                vp_buf_desc.desc = GfxBufferDesc().setByteSize(256).setIsConstantBuffer(true).enableAutomaticStateTracking(GfxResourceStates::ConstantBuffer).setDebugName("SpriteVPBuffer");
                parameters.vp_buffer = pass_builder.write(pass_builder.createTransientBuffer(vp_buf_desc, "SpriteVPBuffer"));
            },
            [pass_context, &view, &resources](const SpritePassParameters& parameters, const RenderGraphPassContext& context, DrawCommandList& command_list) {
                const auto* sprite_ext = view.getExtension<SpriteViewExtension>();
                if (!sprite_ext || sprite_ext->visible_sprites.empty()) {
                    return;
                }

                const auto color_target = context.resolveTexture(parameters.color_target);
                const auto quad_vertex_buffer = context.resolveBuffer(parameters.quad_vertex_buffer);
                const auto quad_index_buffer = context.resolveBuffer(parameters.quad_index_buffer);
                const auto instance_buffer = context.resolveBuffer(parameters.instance_buffer);
                const auto vp_buffer = context.resolveBuffer(parameters.vp_buffer);

                command_list.setTextureState(color_target, GfxAllSubresources, GfxResourceStates::RenderTarget);
                command_list.commitBarriers();

                const auto swapchain_extent = context.getGfxContext()->getSwapchainExtent2d();
                const Matrix4f vp_matrix = Math::OrthoRH_ZO(
                    0.0f, static_cast<Float>(swapchain_extent.x),
                    static_cast<Float>(swapchain_extent.y), 0.0f,
                    0.0f, 1.0f);
                command_list.setBufferState(vp_buffer, GfxResourceStates::CopyDest);
                command_list.commitBarriers();
                command_list.writeBuffer(vp_buffer, &vp_matrix, sizeof(Matrix4f), 0);
                command_list.setBufferState(vp_buffer, GfxResourceStates::ConstantBuffer);
                command_list.commitBarriers();

                command_list.writeBuffer(quad_vertex_buffer, kQuadVertices, sizeof(kQuadVertices), 0);
                command_list.writeBuffer(quad_index_buffer, kQuadIndices, sizeof(kQuadIndices), 0);
                command_list.setBufferState(instance_buffer, GfxResourceStates::CopyDest);
                command_list.commitBarriers();

                DynamicArray<SpriteInstance> instances{};
                instances.reserve(sprite_ext->visible_sprites.size());
                for (const auto* info : sprite_ext->visible_sprites) {
                    instances.push_back(info->toInstance());
                }
                command_list.writeBuffer(instance_buffer, instances.data(), instances.size() * sizeof(SpriteInstance), 0);

                command_list.setBufferState(quad_vertex_buffer, GfxResourceStates::VertexBuffer);
                command_list.setBufferState(quad_index_buffer, GfxResourceStates::IndexBuffer);
                command_list.commitBarriers();

                const UInt32 visible_count = static_cast<UInt32>(sprite_ext->visible_sprites.size());
                resources.renderSprites(pass_context, context, command_list, color_target, quad_vertex_buffer, quad_index_buffer, instance_buffer, vp_buffer, visible_count);
            }
        );
    }

} // namespace dodoe::RenderPipelinePass
