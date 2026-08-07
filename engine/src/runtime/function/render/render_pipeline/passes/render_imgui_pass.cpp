// do@Redlive

#include "render_imgui_pass.h"

#include "render_pass_blackboard_keys.h"

#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_pipeline/render_graph_import_keys.h"
#include "runtime/function/render/render_pipeline/render_graph_import_registry.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/shader/global_samplers.h"

#include <algorithm>

#ifdef DODOE_DEBUG_ENABLED
#include "imgui/imgui.h"
#include "runtime/function/ui/imgui/imgui_builder.h"

namespace dodoe {

    struct ImGuiPassParameters {
        RenderGraphTextureHandle output{};
        RenderGraphTextureHandle font_texture{};
        RenderGraphBufferHandle vertex_buffer{};
        RenderGraphBufferHandle index_buffer{};
    };

    void ImGuiPass::build(RenderGraphBuilder& graph,
                          const RenderPassBuildContext& context) {
        graph.addPass<ImGuiPassParameters>(
            "ImGuiPass",
            RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
            [&context](RenderGraphPassBuilder& pass_builder, ImGuiPassParameters& parameters) {
                const auto swapchain_extent = context.gfx_context->getSwapchainExtent2d();
                RenderGraphTextureDesc color_desc{};
                color_desc.desc = GfxTextureDesc()
                    .setWidth(swapchain_extent.x).setHeight(swapchain_extent.y).setDepth(1)
                    .setFormat(GfxFormat::RGBA8_UNORM)
                    .setIsRenderTarget(true).enableAutomaticStateTracking(GfxResourceStates::RenderTarget)
                    .setDebugName("RDG ImGuiColor");
                RenderGraphAttachmentInfo color_attachment{};
                color_attachment.load_op = LoadOp::Clear;
                color_attachment.clear_color = GfxColor(0.0f, 0.0f, 0.0f, 0.0f);
                parameters.output = pass_builder.writeColor(
                    pass_builder.createTransientTexture(color_desc, "ImGuiColor"), color_attachment);
                pass_builder.blackboard().set<ImGuiColorKey>(parameters.output);

                DO_ASSERT(context.graph_imports != nullptr, "ImGuiPass graph imports are null");
                if (const auto* font_texture = context.graph_imports->find<ImGuiFontTextureKey>();
                    font_texture && *font_texture) {
                    parameters.font_texture = pass_builder.read(
                        pass_builder.importTexture(*font_texture, "ImGuiFontTexture"));
                }

                RenderGraphBufferDesc vb_desc{};
                vb_desc.desc = GfxBufferDesc()
                    .setByteSize(65536 * sizeof(ImDrawVert))
                    .setIsVertexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
                    .setDebugName("RDG ImGuiVB");
                parameters.vertex_buffer = pass_builder.writeBuffer(
                    pass_builder.createTransientBuffer(vb_desc, "ImGuiVertexBuffer"),
                    RenderGraphPipelineStage::Copy);
                pass_builder.readBuffer(parameters.vertex_buffer, RenderGraphPipelineStage::VertexShader);

                RenderGraphBufferDesc ib_desc{};
                ib_desc.desc = GfxBufferDesc()
                    .setByteSize(65536 * sizeof(ImDrawIdx))
                    .setIsIndexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
                    .setDebugName("RDG ImGuiIB");
                parameters.index_buffer = pass_builder.writeBuffer(
                    pass_builder.createTransientBuffer(ib_desc, "ImGuiIndexBuffer"),
                    RenderGraphPipelineStage::Copy);
                pass_builder.readBuffer(parameters.index_buffer, RenderGraphPipelineStage::VertexShader);
            },
            [this](const ImGuiPassParameters& parameters, const RenderGraphPassContext& ctx,
               DrawCommandList& command_list) {
                GfxTextureHandle font_texture{};
                if (parameters.font_texture.isValid()) {
                    font_texture = ctx.resolveTexture(parameters.font_texture);
                }

                const auto& packet = ImGuiBuilder::GetRenderPacket();
                if (packet.lists.empty()) {
                    return;
                }

                UInt32 total_vertex_count = 0;
                UInt32 total_index_count = 0;
                for (const auto& list : packet.lists) {
                    total_vertex_count += static_cast<UInt32>(list.vertices.size());
                    total_index_count += static_cast<UInt32>(list.indices.size());
                }
                if (total_vertex_count == 0 || total_index_count == 0) {
                    return;
                }

                const auto vb = ctx.resolveBuffer(parameters.vertex_buffer);
                const auto ib = ctx.resolveBuffer(parameters.index_buffer);
                const UInt64 vertex_bytes = static_cast<UInt64>(total_vertex_count) * sizeof(ImDrawVert);
                const UInt64 index_bytes = static_cast<UInt64>(total_index_count) * sizeof(ImDrawIdx);
                if (vertex_bytes > vb->getByteSize() || index_bytes > ib->getByteSize()) {
                    DO_ERROR("ImGuiPass: transient UI buffers are too small");
                    return;
                }

                command_list.setBufferState(vb, GfxResourceStates::CopyDest);
                command_list.setBufferState(ib, GfxResourceStates::CopyDest);
                command_list.commitBarriers();

                UInt32 vertex_offset = 0;
                UInt32 index_offset = 0;
                for (const auto& list : packet.lists) {
                    if (!list.vertices.empty()) {
                        command_list.writeBuffer(vb, list.vertices.data(),
                                                 list.vertices.size() * sizeof(ImDrawVert), vertex_offset);
                    }
                    if (!list.indices.empty()) {
                        command_list.writeBuffer(ib, list.indices.data(),
                                                 list.indices.size() * sizeof(ImDrawIdx), index_offset);
                    }
                    vertex_offset += static_cast<UInt32>(list.vertices.size() * sizeof(ImDrawVert));
                    index_offset += static_cast<UInt32>(list.indices.size() * sizeof(ImDrawIdx));
                }

                command_list.setBufferState(vb, GfxResourceStates::VertexBuffer);
                command_list.setBufferState(ib, GfxResourceStates::IndexBuffer);
                command_list.commitBarriers();

                const auto* shader_library = ctx.getShaderLibrary();
                const auto* pipeline_cache = ctx.getPipelineStateCache();
                if (!shader_library || !pipeline_cache || !m_binding_layout || !m_push_layout || !m_input_layout) {
                    DO_ERROR("ImGuiPass: shader or pipeline resources are unavailable");
                    return;
                }

                auto pipeline_desc = GfxGraphicsPipelineDesc()
                    .setVertexShader(shader_library->getImGuiVertexShader())
                    .setPixelShader(shader_library->getImGuiPixelShader())
                    .setInputLayout(m_input_layout)
                    .addBindingLayout(m_binding_layout)
                    .addBindingLayout(m_push_layout)
                    .setPrimType(GfxPrimitiveType::TriangleList);
                GfxDepthStencilState depth_stencil;
                depth_stencil.disableDepthTest().disableDepthWrite().disableStencil();
                GfxRasterState raster;
                raster.setCullNone();
                GfxBlendState blend;
                GfxBlendState::RenderTarget blend_target;
                blend_target.enableBlend()
                    .setSrcBlend(GfxBlendFactor::SrcAlpha)
                    .setDestBlend(GfxBlendFactor::OneMinusSrcAlpha)
                    .setSrcBlendAlpha(GfxBlendFactor::One)
                    .setDestBlendAlpha(GfxBlendFactor::OneMinusSrcAlpha);
                blend.setRenderTarget(0, blend_target);
                GfxRenderState render_state;
                render_state.setDepthStencilState(depth_stencil).setRasterState(raster).setBlendState(blend);
                pipeline_desc.setRenderState(render_state);

                const auto pipeline = pipeline_cache->resolveGraphicsPipeline(
                    pipeline_desc, ctx.getRenderTargetSignature(), command_list);
                if (!pipeline) {
                    DO_ERROR("ImGuiPass: failed to create pipeline");
                    return;
                }

                const Float left = packet.display_pos.x;
                const Float right = packet.display_pos.x + packet.display_size.x;
                const Float top = packet.display_pos.y;
                const Float bottom = packet.display_pos.y + packet.display_size.y;
                if (right <= left || bottom <= top) {
                    return;
                }

                struct ImGuiPushConstants {
                    Float inv_display_size[2];
                    Float display_origin[2];
                } push_data{
                    {1.0f / (right - left), 1.0f / (bottom - top)},
                    {left, top}};

                const auto framebuffer = ctx.getFramebuffer();
                UInt32 global_vertex_offset = 0;
                UInt32 global_index_offset = 0;
                for (const auto& list : packet.lists) {
                    for (const auto& draw : list.commands) {
                        if (draw.user_callback) {
                            continue;
                        }
                        if (!draw.texture_id) {
                            continue;
                        }

                        auto* texture = reinterpret_cast<GfxTexture*>(draw.texture_id);
                        if (!texture || !texture->isRHIReady()) {
                            continue;
                        }
                        const auto binding_set = texture == font_texture.get() && m_font_binding_set
                            ? m_font_binding_set
                            : command_list.createBindingSet(
                                GfxBindingSetDesc()
                                    .addItem(GfxBindingSetItem::Texture_SRV(1, texture->getRHI()))
                                    .addItem(GfxBindingSetItem::Sampler(9, GlobalSamplers::screen().Get())),
                                m_binding_layout);
                        if (!binding_set) {
                            continue;
                        }

                        const Float clip_x = std::max(draw.clip_rect.x - packet.display_pos.x, 0.0f);
                        const Float clip_y = std::max(draw.clip_rect.y - packet.display_pos.y, 0.0f);
                        const Float clip_z = std::min(draw.clip_rect.z - packet.display_pos.x, packet.display_size.x);
                        const Float clip_w = std::min(draw.clip_rect.w - packet.display_pos.y, packet.display_size.y);
                        if (clip_z <= clip_x || clip_w <= clip_y) {
                            continue;
                        }

                        GfxViewportState viewport;
                        viewport.addViewport(GfxViewport(left, right, top, bottom, 0.0f, 1.0f));
                        viewport.addScissorRect(GfxRect(
                            static_cast<Int32>(clip_x), static_cast<Int32>(clip_z),
                            static_cast<Int32>(clip_y), static_cast<Int32>(clip_w)));

                        DynamicArray<GfxBindingSetHandle> binding_sets = {binding_set};
                        DynamicArray<GfxVertexBufferBinding> vertex_buffers = {
                            GfxVertexBufferBinding().setBuffer(vb->getRHIHandle()).setSlot(0).setOffset(global_vertex_offset)};
                        command_list.setGraphicsState(framebuffer, pipeline, binding_sets, viewport, vertex_buffers,
                            GfxIndexBufferBinding().setBuffer(ib->getRHIHandle()).setFormat(GfxFormat::R16_UINT).setOffset(global_index_offset));
                        command_list.setPushConstants(&push_data, sizeof(push_data));
                        command_list.drawIndexed(GfxDrawArguments()
                            .setVertexCount(draw.elem_count)
                            .setStartIndexLocation(draw.idx_offset)
                            .setStartVertexLocation(draw.vtx_offset));
                    }
                    global_vertex_offset += static_cast<UInt32>(list.vertices.size() * sizeof(ImDrawVert));
                    global_index_offset += static_cast<UInt32>(list.indices.size() * sizeof(ImDrawIdx));
                }
            });
    }

} // namespace dodoe

#endif
