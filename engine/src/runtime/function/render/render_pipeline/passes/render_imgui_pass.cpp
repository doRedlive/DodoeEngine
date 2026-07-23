// do@Redlive

#include "render_imgui_pass.h"

#include "render_pass_blackboard_keys.h"
#include "../render_pipeline_pass_utils.h"

#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/mesh_draw/local_vertex_factory.h"
#include "runtime/function/render/mesh_draw/local_vertex_factory.h"

#ifdef DODOE_DEBUG_ENABLED
#include "imgui/imgui.h"
#include "runtime/function/ui/imgui/imgui_builder.h"

namespace dodoe {

    struct ImGuiPassParameters {
        RenderGraphTextureHandle output{};
        RenderGraphBufferHandle vertex_buffer{};
        RenderGraphBufferHandle index_buffer{};
    };

    void ImGuiPass::build(RenderGraphBuilder& graph,
                           const RenderPassBuildContext& context) {
        const auto& pass_context = context.pass_context;
        DO_ASSERT(pass_context.isValid(), "RenderingPipeline pass context is invalid");

        graph.addPass<ImGuiPassParameters>(
            "ImGuiPass",
            RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
            [pass_context](RenderGraphPassBuilder& pass_builder, ImGuiPassParameters& parameters) {
                const auto swapchain_extent = pass_context.gfx_context->getSwapchainExtent2d();
                RenderGraphTextureDesc color_desc{};
                color_desc.desc = GfxTextureDesc()
                    .setWidth(swapchain_extent.x).setHeight(swapchain_extent.y).setDepth(1)
                    .setFormat(GfxFormat::RGBA8_UNORM)
                    .setUsage(GfxResourceStates::RenderTarget)
                    .setDebugName("RDG ImGuiColor");
                parameters.output = pass_builder.write(pass_builder.createTransientTexture(color_desc, "ImGuiColor"));
                pass_builder.blackboard().set<ImGuiColorKey>(parameters.output);

                RenderGraphBufferDesc vb_desc{};
                vb_desc.desc = GfxBufferDesc()
                    .setByteSize(65536 * sizeof(ImDrawVert))
                    .setIsVertexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
                    .setDebugName("RDG ImGuiVB");
                parameters.vertex_buffer = pass_builder.write(pass_builder.createTransientBuffer(vb_desc, "ImGuiVertexBuffer"));

                RenderGraphBufferDesc ib_desc{};
                ib_desc.desc = GfxBufferDesc()
                    .setByteSize(65536 * sizeof(ImDrawIdx))
                    .setIsIndexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
                    .setDebugName("RDG ImGuiIB");
                parameters.index_buffer = pass_builder.write(pass_builder.createTransientBuffer(ib_desc, "ImGuiIndexBuffer"));
            },
            [pass_context](const ImGuiPassParameters& parameters, const RenderGraphPassContext& ctx, DrawCommandList& command_list) {
                ImGui::Render();
                auto* draw_data = ImGui::GetDrawData();
                if (!draw_data || draw_data->TotalVtxCount == 0) {
                    return;
                }

                const auto color_target = ctx.resolveTexture(parameters.output);
                const auto vb = ctx.resolveBuffer(parameters.vertex_buffer);
                const auto ib = ctx.resolveBuffer(parameters.index_buffer);

                command_list.setBufferState(vb, GfxResourceStates::CopyDest);
                command_list.setBufferState(ib, GfxResourceStates::CopyDest);
                command_list.commitBarriers();

                UInt32 vb_offset = 0, ib_offset = 0;
                for (int n = 0; n < draw_data->CmdListsCount; n++) {
                    const auto* cmd_list = draw_data->CmdLists[n];
                    command_list.writeBuffer(vb, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert), vb_offset);
                    command_list.writeBuffer(ib, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx), ib_offset);
                    vb_offset += cmd_list->VtxBuffer.Size * sizeof(ImDrawVert);
                    ib_offset += cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx);
                }

                command_list.setBufferState(vb, GfxResourceStates::VertexBuffer);
                command_list.setBufferState(ib, GfxResourceStates::IndexBuffer);
                command_list.commitBarriers();

                auto framebuffer_desc = GfxFramebufferDesc().addColorAttachment(color_target);
                auto framebuffer = command_list.createFramebuffer(framebuffer_desc);

                const auto* shader_library = pass_context.getShaderLibrary();
                const auto* pso_cache = pass_context.getPipelineStateCache();
                if (!shader_library || !pso_cache) {
                    return;
                }

                const auto vs = shader_library->getImGuiVertexShader();
                const auto ps = shader_library->getImGuiPixelShader();
                if (!vs || !ps) {
                    return;
                }

                GfxInputLayoutHandle input_layout{};
                if (auto* input_cache = pass_context.getSharedRenderService()->getInputLayoutCache()) {
                    input_layout = input_cache->find("ImGui");
                }

                auto pipeline_desc = GfxGraphicsPipelineDesc()
                    .setVertexShader(vs)
                    .setPixelShader(ps)
                    .setPrimType(GfxPrimitiveType::TriangleList);
                if (input_layout) {
                    pipeline_desc.setInputLayout(input_layout);
                }

                GfxDepthStencilState ds;
                ds.disableDepthTest().disableDepthWrite().disableStencil();
                GfxRasterState raster;
                raster.setCullNone();
                GfxBlendState blend;
                blend.enableBlend().setColorBlendFunc(GfxBlendFactor::SrcAlpha, GfxBlendFactor::OneMinusSrcAlpha);
                GfxRenderState render_state;
                render_state.setDepthStencilState(ds).setRasterState(raster).setBlendState(blend);
                pipeline_desc.setRenderState(render_state);

                GfxFramebufferInfo framebuffer_info(framebuffer_desc);
                auto pipeline = pso_cache->resolveGraphicsPipeline(pipeline_desc, framebuffer_info, command_list);
                if (!pipeline) {
                    return;
                }

                command_list.setTextureState(color_target, GfxAllSubresources, GfxResourceStates::RenderTarget);
                command_list.commitBarriers();

                const auto swapchain_extent = ctx.getGfxContext()->getSwapchainExtent2d();
                const float L = draw_data->DisplayPos.x;
                const float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
                const float T = draw_data->DisplayPos.y;
                const float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;

                struct ImGuiPushConstants {
                    float scale[2];
                    float translate[2];
                } push_data;
                push_data.scale[0] = 2.0f / (R - L);
                push_data.scale[1] = 2.0f / (T - B);
                push_data.translate[0] = -(R + L) / (R - L);
                push_data.translate[1] = -(T + B) / (T - B);

                UInt32 global_vb_offset = 0, global_ib_offset = 0;
                for (int n = 0; n < draw_data->CmdListsCount; n++) {
                    const auto* cmd_list = draw_data->CmdLists[n];
                    for (int i = 0; i < cmd_list->CmdBuffer.Size; i++) {
                        const auto& draw = cmd_list->CmdBuffer[i];
                        if (draw.UserCallback) {
                            draw.UserCallback(cmd_list, &draw);
                            continue;
                        }

                        auto vp = GfxViewportState();
                        vp.addViewport(GfxViewport(L, R, T, B, 0.0f, 1.0f));
                        vp.addScissorRect(GfxRectangle(
                            static_cast<Int32>(draw.ClipRect.x - draw_data->DisplayPos.x),
                            static_cast<Int32>(draw.ClipRect.y - draw_data->DisplayPos.y),
                            static_cast<Int32>(draw.ClipRect.z - draw.ClipRect.x),
                            static_cast<Int32>(draw.ClipRect.w - draw.ClipRect.y)));

                        GfxBindingSetHandle tex_set{};
                        if (draw.TextureId) {
                            auto* gfx_tex = static_cast<GfxTexture*>(draw.TextureId);
                            tex_set = command_list.createBindingSet(
                                GfxBindingSetDesc().addItem(
                                    GfxBindingSetItem::Texture_SRV(0, gfx_tex ? gfx_tex->getRHIHandle() : GfxTextureHandle{})),
                                pipeline_desc.getBindingLayout(0));
                        }

                        DynamicArray<GfxBindingSetHandle> bs_arr;
                        if (tex_set) {
                            bs_arr.push_back(tex_set);
                        }

                        DynamicArray<GfxVertexBufferBinding> vbs;
                        vbs.push_back(GfxVertexBufferBinding()
                            .setBuffer(vb->getRHIHandle()).setSlot(0).setOffset(global_vb_offset));
                        command_list.setIndexBuffer(GfxIndexBufferBinding()
                            .setBuffer(ib->getRHIHandle()).setOffset(global_ib_offset));

                        command_list.setGraphicsState(framebuffer, pipeline, bs_arr, vp, vbs);
                        command_list.setPushConstants(GfxShaderType::Vertex, &push_data, sizeof(push_data));
                        command_list.drawIndexed(draw.ElemCount, draw.IdxOffset, draw.VtxOffset);

                        global_ib_offset += cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx);
                    }
                    global_vb_offset += cmd_list->VtxBuffer.Size * sizeof(ImDrawVert);
                }

                command_list.setTextureState(color_target, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.commitBarriers();
            }
        );
    }

} // namespace dodoe

#endif
