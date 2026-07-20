// do@Redlive

#include "render_pipeline_passes.h"

#include "render_pass_blackboard_keys.h"
#include "../render_pipeline_pass_utils.h"

#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/mesh_draw/local_vertex_factory.h"
#include "runtime/function/render/render_pipeline/render_feature/imgui_render_resource.h"

#ifdef DODOE_DEBUG_ENABLED
#include "imgui/imgui.h"
#include "runtime/function/ui/imgui/imgui_builder.h"

#endif

namespace dodoe::RenderPipelinePass {

    inline constexpr UInt32 kImGuiMaxVertices = 65536;
    inline constexpr UInt32 kImGuiMaxIndices  = 65536;

    struct ImGuiPassParameters {
        RenderGraphTextureHandle output{};
        RenderGraphBufferHandle  vertex_buffer{};
        RenderGraphBufferHandle  index_buffer{};
    };

    struct ImGuiConstantBuffer {
        float inv_display_size[2]{};
        float display_pos[2]{};
    };

    void RenderImGuiPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context, ImGuiRenderResource& resources) {
#ifdef DODOE_DEBUG_ENABLED
        if (!ImGui::GetCurrentContext()) return;

        graph.addPass<ImGuiPassParameters>(
            "ImGuiPass",
            RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
            [pass_context](RenderGraphPassBuilder& pass_builder, ImGuiPassParameters& parameters) {
                const auto swapchain_extent = pass_context.gfx_context->getSwapchainExtent2d();
                parameters.output = pass_builder.write(pass_builder.createTransientTexture(
                    rendering_pipeline_utils::MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA8_UNORM, "RDG ImGuiColor"),
                    "ImGuiColor"));
                pass_builder.blackboard().set<ImGuiColorKey>(parameters.output);

                RenderGraphBufferDesc vb_desc{};
                vb_desc.desc = GfxBufferDesc()
                    .setByteSize(kImGuiMaxVertices * static_cast<UInt32>(sizeof(ImDrawVert)))
                    .setIsVertexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
                    .setDebugName("RDG ImGuiVertexBuffer");
                parameters.vertex_buffer = pass_builder.write(pass_builder.createTransientBuffer(vb_desc, "ImGuiVertexBuffer"));

                RenderGraphBufferDesc ib_desc{};
                ib_desc.desc = GfxBufferDesc()
                    .setByteSize(kImGuiMaxIndices * static_cast<UInt32>(sizeof(ImDrawIdx)))
                    .setIsIndexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
                    .setDebugName("RDG ImGuiIndexBuffer");
                parameters.index_buffer = pass_builder.write(pass_builder.createTransientBuffer(ib_desc, "ImGuiIndexBuffer"));
            },
            [pass_context, &resources](const ImGuiPassParameters& parameters, const RenderGraphPassContext& context, DrawCommandList& command_list) {
                const auto output = context.resolveTexture(parameters.output);
                const auto* gfx_context = context.getGfxContext();
                const auto* shader_library = pass_context.getShaderLibrary();
                auto* pipeline_cache = pass_context.getPipelineStateCache();

                ImGuiContext* prev_ctx = ImGui::GetCurrentContext();
                ImGui::SetCurrentContext(ImGuiBuilder::GetContext());
                if (!ImGui::GetCurrentContext()) {
                    command_list.setTextureState(output, GfxAllSubresources, GfxResourceStates::RenderTarget);
                    command_list.commitBarriers();
                    command_list.clearTextureFloat(output, GfxAllSubresources, GfxColor(0.0f, 0.0f, 0.0f, 0.0f));
                    command_list.setTextureState(output, GfxAllSubresources, GfxResourceStates::ShaderResource);
                    command_list.commitBarriers();
                    return;
                }

                resources.getOrCreateFontTexture(command_list);

                auto input_layout = pass_context.local_vertex_factory
                    ? pass_context.local_vertex_factory->getOrCreateImGuiInputLayout(command_list, shader_library->getImGuiVertexShader())
                    : GfxInputLayoutHandle{};

                auto binding_layout = resources.getOrCreateBindingLayout(command_list);
                auto framebuffer = resources.getOrCreateFramebuffer(command_list, output);

                auto pipeline = resources.getOrCreatePipeline(
                    pipeline_cache,
                    shader_library->getImGuiVertexShader(),
                    shader_library->getImGuiPixelShader(),
                    input_layout,
                    framebuffer->getInfo(),
                    command_list);

                ImGui::Render();
                ImDrawData* draw_data = ImGui::GetDrawData();
                if (!draw_data || !pipeline || !framebuffer || !binding_layout) {
                    command_list.setTextureState(output, GfxAllSubresources, GfxResourceStates::RenderTarget);
                    command_list.commitBarriers();
                    command_list.clearTextureFloat(output, GfxAllSubresources, GfxColor(0.0f, 0.0f, 0.0f, 0.0f));
                    command_list.setTextureState(output, GfxAllSubresources, GfxResourceStates::ShaderResource);
                    command_list.commitBarriers();
                    ImGui::SetCurrentContext(prev_ctx);
                    return;
                }

                const int fb_width = static_cast<int>(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
                const int fb_height = static_cast<int>(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
                command_list.setTextureState(output, GfxAllSubresources, GfxResourceStates::RenderTarget);
                command_list.commitBarriers();
                command_list.clearTextureFloat(output, GfxAllSubresources, GfxColor(0.0f, 0.0f, 0.0f, 0.0f));

                if (fb_width <= 0 || fb_height <= 0 || draw_data->TotalVtxCount <= 0 || draw_data->TotalIdxCount <= 0) {
                    command_list.setTextureState(output, GfxAllSubresources, GfxResourceStates::ShaderResource);
                    command_list.commitBarriers();
                    ImGui::SetCurrentContext(prev_ctx);
                    return;
                }

                ImGuiConstantBuffer cb_data;
                cb_data.inv_display_size[0] = 1.0f / draw_data->DisplaySize.x;
                cb_data.inv_display_size[1] = 1.0f / draw_data->DisplaySize.y;
                cb_data.display_pos[0] = draw_data->DisplayPos.x;
                cb_data.display_pos[1] = draw_data->DisplayPos.y;

                const size_t vertex_byte_size = static_cast<size_t>(draw_data->TotalVtxCount) * sizeof(ImDrawVert);
                const size_t index_byte_size = static_cast<size_t>(draw_data->TotalIdxCount) * sizeof(ImDrawIdx);

                const auto vertex_buffer = context.resolveBuffer(parameters.vertex_buffer);
                const auto index_buffer  = context.resolveBuffer(parameters.index_buffer);

                DO_ASSERT(vertex_byte_size <= static_cast<size_t>(vertex_buffer->getByteSize()),
                          "ImGui vertex count exceeds RDG buffer capacity");
                DO_ASSERT(index_byte_size <= static_cast<size_t>(index_buffer->getByteSize()),
                          "ImGui index count exceeds RDG buffer capacity");

                DynamicArray<ImDrawVert> vertex_upload(draw_data->TotalVtxCount);
                DynamicArray<ImDrawIdx> index_upload(draw_data->TotalIdxCount);
                {
                    ImDrawVert* vtx_dst = vertex_upload.data();
                    ImDrawIdx* idx_dst = index_upload.data();
                    for (int list_index = 0; list_index < draw_data->CmdListsCount; ++list_index) {
                        const ImDrawList* draw_list = draw_data->CmdLists[list_index];
                        std::memcpy(vtx_dst, draw_list->VtxBuffer.Data, static_cast<size_t>(draw_list->VtxBuffer.Size) * sizeof(ImDrawVert));
                        std::memcpy(idx_dst, draw_list->IdxBuffer.Data, static_cast<size_t>(draw_list->IdxBuffer.Size) * sizeof(ImDrawIdx));
                        vtx_dst += draw_list->VtxBuffer.Size;
                        idx_dst += draw_list->IdxBuffer.Size;
                    }
                }

                command_list.setBufferState(vertex_buffer, GfxResourceStates::CopyDest);
                command_list.commitBarriers();
                command_list.writeBuffer(vertex_buffer, vertex_upload.data(), vertex_byte_size, 0);
                command_list.setBufferState(vertex_buffer, GfxResourceStates::CopyDest);
                command_list.commitBarriers();
                command_list.writeBuffer(index_buffer, index_upload.data(), index_byte_size, 0);
                command_list.setBufferState(vertex_buffer, GfxResourceStates::VertexBuffer);
                command_list.setBufferState(index_buffer, GfxResourceStates::IndexBuffer);
                command_list.commitBarriers();

                GfxViewportState vp;
                vp.viewports.push_back(GfxViewport(static_cast<float>(fb_width), static_cast<float>(fb_height)));
                vp.scissorRects.resize(1);
                vp.scissorRects[0] = GfxRect(fb_width, fb_height);  // fullscreen default to avoid zero-area scissor

                DynamicArray<GfxVertexBufferBinding> vbs;
                vbs.push_back(GfxVertexBufferBinding().setBuffer(vertex_buffer->getRHIHandle()).setSlot(0).setOffset(0));
                GfxIndexBufferBinding ib = GfxIndexBufferBinding()
                    .setBuffer(index_buffer->getRHIHandle())
                    .setFormat(sizeof(ImDrawIdx) == 2 ? GfxFormat::R16_UINT : GfxFormat::R32_UINT)
                    .setOffset(0);

                const ImVec2 clip_off = draw_data->DisplayPos;
                const ImVec2 clip_scale = draw_data->FramebufferScale;
                int vtx_offset = 0;
                int idx_offset = 0;
                for (int list_index = 0; list_index < draw_data->CmdListsCount; ++list_index) {
                    const ImDrawList* draw_list = draw_data->CmdLists[list_index];
                    for (int cmd_index = 0; cmd_index < draw_list->CmdBuffer.Size; ++cmd_index) {
                        const ImDrawCmd* draw_cmd = &draw_list->CmdBuffer[cmd_index];
                        if (draw_cmd->UserCallback) {
                            draw_cmd->UserCallback(draw_list, draw_cmd);
                            continue;
                        }

                        ImVec2 clip_min((draw_cmd->ClipRect.x - clip_off.x) * clip_scale.x, (draw_cmd->ClipRect.y - clip_off.y) * clip_scale.y);
                        ImVec2 clip_max((draw_cmd->ClipRect.z - clip_off.x) * clip_scale.x, (draw_cmd->ClipRect.w - clip_off.y) * clip_scale.y);
                        clip_min.x = std::clamp(clip_min.x, 0.0f, static_cast<float>(fb_width));
                        clip_min.y = std::clamp(clip_min.y, 0.0f, static_cast<float>(fb_height));
                        clip_max.x = std::clamp(clip_max.x, 0.0f, static_cast<float>(fb_width));
                        clip_max.y = std::clamp(clip_max.y, 0.0f, static_cast<float>(fb_height));
                        if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y) {
                            continue;
                        }

                        auto* texture = reinterpret_cast<GfxTexture*>(draw_cmd->GetTexID());
                        texture = texture ? texture : resources.getOrCreateFontTexture(command_list).get();
                        if (!texture) {
                            continue;
                        }

                        auto binding_set = resources.getOrCreateBindingSet(command_list, texture);
                        if (!binding_set) {
                            continue;
                        }

                        vp.scissorRects[0] = GfxRect(
                            static_cast<int>(clip_min.x), static_cast<int>(clip_max.x),
                            static_cast<int>(clip_min.y), static_cast<int>(clip_max.y));
                        DynamicArray<GfxBindingSetHandle> bs_arr = {binding_set};
                        command_list.setGraphicsState(framebuffer, pipeline, bs_arr, vp, vbs, ib);
                        command_list.setPushConstants(&cb_data, sizeof(cb_data));
                        command_list.drawIndexed(
                            GfxDrawArguments()
                                .setVertexCount(draw_cmd->ElemCount)
                                .setInstanceCount(1)
                                .setStartIndexLocation(static_cast<UInt32>(idx_offset + static_cast<int>(draw_cmd->IdxOffset)))
                                .setStartVertexLocation(static_cast<UInt32>(vtx_offset + static_cast<int>(draw_cmd->VtxOffset))));
                    }
                    idx_offset += draw_list->IdxBuffer.Size;
                    vtx_offset += draw_list->VtxBuffer.Size;
                }

                command_list.setTextureState(output, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.commitBarriers();
                ImGui::SetCurrentContext(prev_ctx);
            }
        );
#else
        (void)graph;
        (void)pass_context;
        (void)resources;
#endif
    }

} // namespace dodoe::RenderPipelinePass
