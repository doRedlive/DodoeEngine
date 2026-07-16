// do@Redlive

#include "dopch.h"

#ifdef DODOE_EDITOR_ENABLED

#include "render_pipeline_passes.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "../render_pipeline_pass_utils.h"

#include "runtime/function/render/framework/pipeline_state_cache.h"
#include "runtime/function/render/framework/shader_library.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_pipeline/render_feature/gizmo_render_resource.h"
#include "runtime/core/channel/gizmo_channel.h"
#include "render_pass_blackboard_keys.h"

namespace dodoe::RenderPipelinePass {

    inline constexpr UInt32 kGizmoMaxVertices = 65536;
    inline constexpr UInt32 kGizmoMaxIndices  = 65536;

    struct GizmoConstantBuffer {
        Matrix4f mvp;
    };

    struct GizmoPassParameters {
        RenderGraphTextureHandle color_target{};
        RenderGraphBufferHandle  vertex_buffer{};
        RenderGraphBufferHandle  index_buffer{};
    };

    void RenderGizmoPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context, GizmoRenderResource& resources) {
        DO_ASSERT(pass_context.isValid(), "GizmoPass: pass context is invalid");

        auto& channel_data = GetGizmoChannel().get<GizmoChannelData>();
        if (!channel_data.has_data || channel_data.commands.empty()) return;

        graph.addPass<GizmoPassParameters>(
            "GizmoPass",
            RenderGraphPassFlags::Raster,
            [pass_context](RenderGraphPassBuilder& pass_builder, GizmoPassParameters& parameters) {
                const auto* scene_color = pass_builder.blackboard().get<SceneColorKey, RenderGraphTextureHandle>();
                if (scene_color) {
                    parameters.color_target = pass_builder.write(*scene_color);
                } else {
                    const auto swapchain_extent = pass_context.gfx_context->getSwapchainExtent2d();
                    parameters.color_target = pass_builder.write(pass_builder.createTransientTexture(
                        rendering_pipeline_utils::MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA8_UNORM, "RDG GizmoColor"),
                        "GizmoColor"));
                    pass_builder.blackboard().set<SceneColorKey>(parameters.color_target);
                }

                RenderGraphBufferDesc vb_desc{};
                vb_desc.desc = GfxBufferDesc()
                    .setByteSize(kGizmoMaxVertices * static_cast<UInt32>(sizeof(GizmoVertex)))
                    .setIsVertexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
                    .setDebugName("RDG GizmoVB");
                parameters.vertex_buffer = pass_builder.write(pass_builder.createTransientBuffer(vb_desc, "GizmoVB"));

                RenderGraphBufferDesc ib_desc{};
                ib_desc.desc = GfxBufferDesc()
                    .setByteSize(kGizmoMaxIndices * static_cast<UInt32>(sizeof(UInt32)))
                    .setIsIndexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
                    .setDebugName("RDG GizmoIB");
                parameters.index_buffer = pass_builder.write(pass_builder.createTransientBuffer(ib_desc, "GizmoIB"));
            },
            [pass_context, &resources](const GizmoPassParameters& parameters, const RenderGraphPassContext& context, DrawCommandList& command_list) {
                auto& channel_data = GetGizmoChannel().get<GizmoChannelData>();
                if (!channel_data.has_data || channel_data.commands.empty()) return;

                const auto color_target = context.resolveTexture(parameters.color_target);
                const auto vertex_buffer = context.resolveBuffer(parameters.vertex_buffer);
                const auto index_buffer  = context.resolveBuffer(parameters.index_buffer);

                const size_t vertex_byte_size = channel_data.vertices.size() * sizeof(GizmoVertex);
                const size_t index_byte_size  = channel_data.indices.size() * sizeof(UInt32);

                if (vertex_byte_size == 0 || index_byte_size == 0) return;

                DO_ASSERT(vertex_byte_size <= static_cast<size_t>(vertex_buffer->getByteSize()),
                          "Gizmo vertex count exceeds RDG buffer capacity");
                DO_ASSERT(index_byte_size <= static_cast<size_t>(index_buffer->getByteSize()),
                          "Gizmo index count exceeds RDG buffer capacity");

                // Upload vertex and index data
                command_list.setBufferState(vertex_buffer, GfxResourceStates::CopyDest);
                command_list.setBufferState(index_buffer, GfxResourceStates::CopyDest);
                command_list.commitBarriers();
                command_list.writeBuffer(vertex_buffer, channel_data.vertices.data(), vertex_byte_size, 0);
                command_list.writeBuffer(index_buffer, channel_data.indices.data(), index_byte_size, 0);
                command_list.setBufferState(vertex_buffer, GfxResourceStates::VertexBuffer);
                command_list.setBufferState(index_buffer, GfxResourceStates::IndexBuffer);
                command_list.commitBarriers();

                const auto* shader_library = pass_context.getShaderLibrary();
                if (!shader_library) {
                    DO_ERROR("GizmoPass: shader_library is null");
                    return;
                }

                const auto gizmo_vs = shader_library->getGizmoVertexShader();
                const auto gizmo_ps = shader_library->getGizmoPixelShader();
                if (!gizmo_vs || !gizmo_ps) {
                    DO_ERROR("GizmoPass: gizmo shaders not loaded");
                    return;
                }

                auto binding_layout = resources.getOrCreateBindingLayout(command_list);
                if (!binding_layout) {
                    DO_ERROR("GizmoPass: failed to create binding layout");
                    return;
                }

                constexpr UInt32 kVertexStride = sizeof(GizmoVertex);
                GfxVertexAttributeDesc attribs[] = {
                    GfxVertexAttributeDesc().setName("POSITION").setFormat(GfxFormat::RGB32_FLOAT).setOffset(0).setElementStride(kVertexStride),
                    GfxVertexAttributeDesc().setName("COLOR").setFormat(GfxFormat::RGBA32_FLOAT).setOffset(12).setElementStride(kVertexStride),
                };
                auto input_layout = command_list.createInputLayout(attribs, 2, gizmo_vs);

                auto framebuffer_desc = GfxFramebufferDesc().addColorAttachment(color_target);
                auto fb = command_list.createFramebuffer(framebuffer_desc);

                GfxDepthStencilState ds;
                ds.disableDepthTest().disableDepthWrite().disableStencil();
                GfxRasterState raster;
                raster.setCullNone();
                GfxBlendState blend;
                blend.enableAlphaBlend();
                GfxRenderState render_state;
                render_state.setDepthStencilState(ds).setRasterState(raster).setBlendState(blend);

                auto* pipeline_cache = pass_context.getPipelineStateCache();
                if (!pipeline_cache) {
                    DO_ERROR("GizmoPass: pipeline_cache is null");
                    return;
                }

                const auto* view = context.getView();
                const auto swapchain_extent = context.getGfxContext()->getSwapchainExtent2d();
                auto vp = GfxViewportState().addViewportAndScissorRect(GfxViewport(
                    0, static_cast<float>(swapchain_extent.x),
                    0, static_cast<float>(swapchain_extent.y),
                    0, 1));

                command_list.setTextureState(color_target, GfxAllSubresources, GfxResourceStates::RenderTarget);
                command_list.commitBarriers();

                DynamicArray<GfxVertexBufferBinding> vbs;
                vbs.push_back(GfxVertexBufferBinding()
                    .setBuffer(vertex_buffer->getRHIHandle()).setSlot(0).setOffset(0));

                GfxIndexBufferBinding ib = GfxIndexBufferBinding()
                    .setBuffer(index_buffer->getRHIHandle())
                    .setFormat(GfxFormat::R32_UINT)
                    .setOffset(0);

                auto pipeline_desc_base = GfxGraphicsPipelineDesc()
                    .setVertexShader(gizmo_vs)
                    .setPixelShader(gizmo_ps)
                    .setInputLayout(input_layout)
                    .addBindingLayout(binding_layout)
                    .setRenderState(render_state);

                GfxFramebufferInfo framebuffer_info(framebuffer_desc);

                GizmoConstantBuffer cb_data{};
                if (view) {
                    cb_data.mvp = view->getProjectionMatrix() * view->getViewMatrix();
                } else {
                    cb_data.mvp = Matrix4f(1.0f);
                }

                for (const auto& cmd : channel_data.commands) {
                    if (cmd.vertex_count == 0) continue;

                    auto pipeline_desc = pipeline_desc_base;
                    pipeline_desc.setPrimType(cmd.topology);

                    auto pipeline = pipeline_cache->resolveGraphicsPipeline(pipeline_desc, framebuffer_info, command_list);
                    if (!pipeline) continue;

                    GizmoConstantBuffer per_cmd_cb;
                    per_cmd_cb.mvp = cb_data.mvp * cmd.transform;

                    DynamicArray<GfxBindingSetHandle> bs_arr = {binding_layout};

                    if (cmd.index_count > 0) {
                        command_list.setGraphicsState(fb, pipeline, bs_arr, vp, vbs, ib);
                        command_list.setPushConstants(&per_cmd_cb, sizeof(per_cmd_cb));
                        command_list.drawIndexed(
                            GfxDrawArguments()
                                .setVertexCount(cmd.index_count)
                                .setInstanceCount(1)
                                .setStartIndexLocation(cmd.index_offset)
                                .setStartVertexLocation(cmd.vertex_offset));
                    } else {
                        command_list.setGraphicsState(fb, pipeline, bs_arr, vp, vbs);
                        command_list.setPushConstants(&per_cmd_cb, sizeof(per_cmd_cb));
                        command_list.draw(
                            GfxDrawArguments()
                                .setVertexCount(cmd.vertex_count)
                                .setInstanceCount(1)
                                .setStartVertexLocation(cmd.vertex_offset));
                    }
                }

                command_list.setTextureState(color_target, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.commitBarriers();
            }
        );
    }

} // namespace dodoe::RenderPipelinePass

#endif // DODOE_EDITOR_ENABLED
