// do@Redlive

#include "dopch.h"

#ifdef DODOE_EDITOR_ENABLED

#include "render_gizmo_pass.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"
#include "runtime/core/channel/gizmo_channel.h"

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
                const auto* scene_color = pass_builder.blackboard().get<SceneColorKey>();
                RenderGraphAttachmentInfo color_attachment{};
                color_attachment.load_op = LoadOp::Load;
                if (scene_color) {
                    parameters.color_target = pass_builder.writeColor(*scene_color, color_attachment);
                } else {
                    const auto swapchain_extent = context.gfx_context->getSwapchainExtent2d();
                    parameters.color_target = pass_builder.writeColor(pass_builder.createTransientTexture(
                        rendering_pipeline_utils::MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA8_UNORM, "RDG GizmoColor"),
                        "GizmoColor"), color_attachment);
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
            [this](const GizmoPassParameters& parameters, const RenderGraphPassContext& ctx, DrawCommandList& command_list) {
                const auto vb = ctx.resolveBuffer(parameters.vertex_buffer);
                const auto ib = ctx.resolveBuffer(parameters.index_buffer);

                struct GizmoPushConstants {
                    Matrix4f mvp;
                    Vector4f color;
                };

                const auto* shader_library = ctx.getShaderLibrary();
                const auto* pso_cache = ctx.getPipelineStateCache();
                if (!shader_library || !pso_cache || !m_binding_layout || !m_input_layout) {
                    return;
                }

                const auto vs = shader_library->getGizmoVertexShader();
                const auto ps = shader_library->getGizmoPixelShader();
                if (!vs || !ps) {
                    return;
                }

                const auto& gizmo_data = GetGizmoChannel().get<GizmoChannelData>();
                if (gizmo_data.vertices.empty() || gizmo_data.commands.empty()) {
                    return;
                }

                GfxDepthStencilState ds;
                ds.enableDepthTest().setDepthFunc(GfxComparisonFunc::Less).disableDepthWrite().disableStencil();
                GfxRasterState raster;
                raster.setCullNone();
                GfxRenderState render_state;
                render_state.setDepthStencilState(ds).setRasterState(raster);

                const UInt64 vb_byte_size = gizmo_data.vertices.size() * sizeof(GizmoVertex);
                const UInt64 ib_byte_size = gizmo_data.indices.size() * sizeof(UInt32);

                command_list.setBufferState(vb, GfxResourceStates::CopyDest);
                if (ib_byte_size > 0) {
                    command_list.setBufferState(ib, GfxResourceStates::CopyDest);
                }
                command_list.commitBarriers();
                command_list.writeBuffer(vb, gizmo_data.vertices.data(), vb_byte_size, 0);
                if (ib_byte_size > 0) {
                    command_list.writeBuffer(ib, gizmo_data.indices.data(), ib_byte_size, 0);
                }
                command_list.setBufferState(vb, GfxResourceStates::VertexBuffer);
                if (ib_byte_size > 0) {
                    command_list.setBufferState(ib, GfxResourceStates::IndexBuffer);
                }
                command_list.commitBarriers();

                const auto view_projection = ctx.getView()->getViewProjectionMatrix();
                const auto framebuffer = ctx.getFramebuffer();

                GizmoPushConstants push{};
                push.color = Vector4f(1.0f, 1.0f, 1.0f, 1.0f);

                for (const auto& cmd : gizmo_data.commands) {
                    if (cmd.vertex_count == 0) {
                        continue;
                    }

                    auto pipeline_desc = GfxGraphicsPipelineDesc()
                        .setVertexShader(vs)
                        .setPixelShader(ps)
                        .setInputLayout(m_input_layout)
                        .addBindingLayout(m_binding_layout)
                        .setPrimType(cmd.topology)
                        .setRenderState(render_state);
                    auto pipeline = pso_cache->resolveGraphicsPipeline(
                        pipeline_desc, ctx.getRenderTargetSignature(), command_list);
                    if (!pipeline) {
                        continue;
                    }

                    push.mvp = view_projection * cmd.transform;

                    DynamicArray<GfxVertexBufferBinding> vbs;
                    vbs.push_back(GfxVertexBufferBinding()
                        .setBuffer(vb->getRHIHandle()).setSlot(0).setOffset(cmd.vertex_offset * sizeof(GizmoVertex)));

                    GfxIndexBufferBinding index_binding;
                    if (cmd.index_count > 0) {
                        index_binding = GfxIndexBufferBinding()
                            .setBuffer(ib->getRHIHandle())
                            .setFormat(GfxFormat::R32_UINT)
                            .setOffset(cmd.index_offset * sizeof(UInt32));
                    }

                    const auto viewport_state = rendering_pipeline_utils::BuildViewportState(*ctx.getView(), ctx.getGfxContext()->getSwapchainExtent2d());
                    command_list.setGraphicsState(framebuffer, pipeline, {}, viewport_state, vbs, index_binding);
                    command_list.setPushConstants(&push, sizeof(push));

                    if (cmd.index_count > 0) {
                        command_list.drawIndexed(GfxDrawArguments().setVertexCount(cmd.index_count));
                    } else {
                        command_list.draw(GfxDrawArguments().setVertexCount(cmd.vertex_count));
                    }
                }
            }
        );
    }

} // namespace dodoe

#endif
