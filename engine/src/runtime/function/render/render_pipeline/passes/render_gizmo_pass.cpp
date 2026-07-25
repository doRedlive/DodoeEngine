// do@Redlive

#include "dopch.h"

#ifdef DODOE_EDITOR_ENABLED

#include "render_gizmo_pass.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "../render_pipeline_pass_utils.h"

#include "runtime/function/render/pipeline/pipeline_state_cache.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "render_pass_blackboard_keys.h"

namespace dodoe {

    struct GizmoPassParameters {
        RenderGraphTextureHandle color_target{};
        RenderGraphBufferHandle vertex_buffer{};
        RenderGraphBufferHandle index_buffer{};
    };

    void GizmoPass::build(RenderGraphBuilder& graph,
                           const RenderPassBuildContext& context) {
        if (!context.view.hasViewFlag(RenderView::kShowEditorPrimitives)) return;

        auto& channel_data = GetGizmoChannel().get<GizmoChannelData>();
        if (!channel_data.has_data || channel_data.commands.empty()) return;

        graph.addPass<GizmoPassParameters>(
            "GizmoPass",
            RenderGraphPassFlags::Raster,
            [&context](RenderGraphPassBuilder& pass_builder, GizmoPassParameters& parameters) {
                const auto* scene_color = pass_builder.blackboard().get<SceneColorKey, RenderGraphTextureHandle>();
                if (scene_color) {
                    parameters.color_target = pass_builder.write(*scene_color);
                } else {
                    const auto swapchain_extent = context.gfx_context->getSwapchainExtent2d();
                    parameters.color_target = pass_builder.write(pass_builder.createTransientTexture(
                        rendering_pipeline_utils::MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA8_UNORM, "RDG GizmoColor"),
                        "GizmoColor"));
                    pass_builder.blackboard().set<SceneColorKey>(parameters.color_target);
                }

                RenderGraphBufferDesc vb_desc{};
                vb_desc.desc = GfxBufferDesc()
                    .setByteSize(65536 * sizeof(GizmoVertex))
                    .setIsVertexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
                    .setDebugName("RDG GizmoVB");
                parameters.vertex_buffer = pass_builder.write(pass_builder.createTransientBuffer(vb_desc, "GizmoVertexBuffer"));

                RenderGraphBufferDesc ib_desc{};
                ib_desc.desc = GfxBufferDesc()
                    .setByteSize(65536 * sizeof(UInt32))
                    .setIsIndexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
                    .setDebugName("RDG GizmoIB");
                parameters.index_buffer = pass_builder.write(pass_builder.createTransientBuffer(ib_desc, "GizmoIndexBuffer"));
            },
            [](const GizmoPassParameters& parameters, const RenderGraphPassContext& ctx, DrawCommandList& command_list) {
                const auto color_target = ctx.resolveTexture(parameters.color_target);
                const auto vb = ctx.resolveBuffer(parameters.vertex_buffer);
                const auto ib = ctx.resolveBuffer(parameters.index_buffer);

                struct GizmoPushConstants {
                    Matrix4f mvp;
                    Vector4f color;
                };

                const auto* shader_library = ctx.getShaderLibrary();
                const auto* pso_cache = ctx.getPipelineStateCache();
                if (!shader_library || !pso_cache) {
                    return;
                }

                const auto vs = shader_library->getGizmoVertexShader();
                const auto ps = shader_library->getGizmoPixelShader();
                if (!vs || !ps) {
                    return;
                }

                GfxDepthStencilState ds;
                ds.enableDepthTest().setDepthFunc(GfxComparisonFunc::Less).disableDepthWrite().disableStencil();
                GfxRasterState raster;
                raster.setCullNone();
                GfxRenderState render_state;
                render_state.setDepthStencilState(ds).setRasterState(raster);

                auto framebuffer_desc = GfxFramebufferDesc().addColorAttachment(color_target);
                auto framebuffer = command_list.createFramebuffer(framebuffer_desc);

                command_list.setTextureState(color_target, GfxAllSubresources, GfxResourceStates::RenderTarget);
                command_list.commitBarriers();

                const auto mvp = ctx.getView()->getViewProjectionMatrix();

                const auto& gizmo_data = GizmoChannel::getDrawData();
                UInt32 vb_offset = 0, ib_offset = 0;
                for (const auto& cmd : gizmo_data.commands) {
                    command_list.setBufferState(vb, GfxResourceStates::CopyDest);
                    command_list.setBufferState(ib, GfxResourceStates::CopyDest);
                    command_list.commitBarriers();
                    command_list.writeBuffer(vb, cmd.vertices.data(), cmd.vertices.size() * sizeof(GizmoVertex), vb_offset);
                    command_list.writeBuffer(ib, cmd.indices.data(), cmd.indices.size() * sizeof(UInt32), ib_offset);
                    command_list.setBufferState(vb, GfxResourceStates::VertexBuffer);
                    command_list.setBufferState(ib, GfxResourceStates::IndexBuffer);
                    command_list.commitBarriers();

                    auto pipeline_desc = GfxGraphicsPipelineDesc()
                        .setVertexShader(vs)
                        .setPixelShader(ps)
                        .setPrimType(cmd.topology);
                    GfxFramebufferInfo fb_info(framebuffer_desc);
                    auto pipeline = pso_cache->resolveGraphicsPipeline(
                        pipeline_desc, fb_info, command_list);
                    if (!pipeline) {
                        continue;
                    }

                    GizmoPushConstants push;
                    push.mvp = mvp;
                    push.color = cmd.color;

                    DynamicArray<GfxVertexBufferBinding> vbs;
                    vbs.push_back(GfxVertexBufferBinding()
                        .setBuffer(vb->getRHIHandle()).setSlot(0).setOffset(vb_offset));
                    command_list.setIndexBuffer(GfxIndexBufferBinding()
                        .setBuffer(ib->getRHIHandle()).setOffset(ib_offset));

                    const auto viewport_state = rendering_pipeline_utils::BuildViewportState(*ctx.getView(), ctx.getGfxContext()->getSwapchainExtent2d());
                    command_list.setGraphicsState(framebuffer, pipeline, {}, viewport_state, vbs);
                    command_list.setPushConstants(GfxShaderType::Vertex, &push, sizeof(push));
                    command_list.drawIndexed(static_cast<UInt32>(cmd.indices.size()), 0, 0);

                    vb_offset += cmd.vertices.size() * sizeof(GizmoVertex);
                    ib_offset += cmd.indices.size() * sizeof(UInt32);
                }

                command_list.setTextureState(color_target, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.commitBarriers();
            }
        );
    }

} // namespace dodoe

#endif
