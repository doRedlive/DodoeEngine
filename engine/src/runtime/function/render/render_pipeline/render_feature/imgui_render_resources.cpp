#include "imgui_render_resources.h"

#include "runtime/function/render/framework/global_samplers.h"
#include "runtime/function/ui/imgui/imgui_builder.h"

#ifdef DODOE_DEBUG
#include "imgui/imgui.h"

#include <algorithm>
#include <cstring>
#endif

namespace dodoe {

#ifdef DODOE_DEBUG
    namespace {
        struct ImGuiPushConstants {
            float inv_display_size[2]{};
            float display_pos[2]{};
        };

        static void ClearOutput(DrawCommandList& command_list, const GfxTextureHandle& output) {
            command_list.setTextureState(output, GfxAllSubresources, GfxResourceStates::RenderTarget);
            command_list.commitBarriers();
            command_list.clearTextureFloat(output, GfxAllSubresources, GfxColor(0.0f, 0.0f, 0.0f, 0.0f));
            command_list.setTextureState(output, GfxAllSubresources, GfxResourceStates::ShaderResource);
            command_list.commitBarriers();
        }
    }
#endif

    void ImGuiRenderResources::reset() {
#ifdef DODOE_DEBUG
        if (ImGui::GetCurrentContext()) {
            ImGui::GetIO().Fonts->SetTexID(ImTextureID_Invalid);
        }
#endif
        m_vertex_buffer = nullptr;
        m_index_buffer = nullptr;
        m_font_texture = nullptr;
        m_input_layout = nullptr;
        m_binding_layout = nullptr;
        m_pipeline = GfxGraphicsPipelineHandle{};
        m_framebuffer = nullptr;
        m_framebuffer_texture = nullptr;
        m_binding_sets.clear();
    }

    void ImGuiRenderResources::renderImGui(
        const RenderPassContext& pass_context,
        const RenderGraphPassContext& context,
        DrawCommandList& command_list,
        const GfxTextureHandle& output)
    {
#ifdef DODOE_DEBUG
        const auto* gfx_context = context.getGfxContext();
        ImGuiContext* prev_ctx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(ImGuiBuilder::GetContext());
        if (!ImGui::GetCurrentContext()) {
            ClearOutput(command_list, output);
            return;
        }

        if (!m_font_texture) {
            ImGuiIO& io = ImGui::GetIO();
            unsigned char* pixels = nullptr;
            int width = 0;
            int height = 0;
            io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
            if (pixels && width > 0 && height > 0) {
                GfxTextureDesc font_desc = GfxTextureDesc()
                    .setDimension(GfxTextureDimension::Texture2D)
                    .setWidth(width)
                    .setHeight(height)
                    .setFormat(GfxFormat::RGBA8_UNORM)
                    .setMipLevels(1)
                    .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
                    .setDebugName("ImGui Font Texture");
                m_font_texture = command_list.createTexture(font_desc, pixels, static_cast<Size_t>(width) * height * 4u);
                io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(m_font_texture.get()));
            }
        }

        const auto* shader_library = pass_context.getShaderLibrary();
        const auto device = gfx_context->getDevice();
        if (!m_input_layout && shader_library) {
            GfxVertexAttributeDesc attributes[] = {
                GfxVertexAttributeDesc().setName("a_Position").setFormat(GfxFormat::RG32_FLOAT).setOffset(offsetof(ImDrawVert, pos)).setElementStride(sizeof(ImDrawVert)),
                GfxVertexAttributeDesc().setName("a_UV").setFormat(GfxFormat::RG32_FLOAT).setOffset(offsetof(ImDrawVert, uv)).setElementStride(sizeof(ImDrawVert)),
                GfxVertexAttributeDesc().setName("a_Color").setFormat(GfxFormat::RGBA8_UNORM).setOffset(offsetof(ImDrawVert, col)).setElementStride(sizeof(ImDrawVert)),
            };
            m_input_layout = device->createInputLayout(attributes, static_cast<UInt32>(std::size(attributes)), shader_library->getImGuiVertexShader());
        }

        if (!m_binding_layout) {
            m_binding_layout = device->createBindingLayout(
                GfxBindingLayoutDesc()
                    .setVisibility(GfxShaderType::All)
                    .addItem(GfxBindingLayoutItem::PushConstants(0, sizeof(ImGuiPushConstants)))
                    .addItem(GfxBindingLayoutItem::Texture_SRV(0))
                    .addItem(GfxBindingLayoutItem::Sampler(0)));
        }

        if (!m_framebuffer || m_framebuffer_texture.get() != output.get()) {
            m_framebuffer_texture = output;
            m_framebuffer = command_list.createFramebuffer(
                GfxFramebufferDesc().addColorAttachment(output));
            m_pipeline = GfxGraphicsPipelineHandle{};
        }

        auto* pipeline_state_cache = pass_context.getPipelineStateCache();
        if (!m_pipeline && m_framebuffer && m_input_layout && shader_library && pipeline_state_cache && m_binding_layout) {
            GfxDepthStencilState depth_stencil_state;
            depth_stencil_state.disableDepthTest().disableDepthWrite().disableStencil();

            GfxBlendState blend_state;
            blend_state.targets[0]
                .enableBlend()
                .setSrcBlend(GfxBlendFactor::SrcAlpha)
                .setDestBlend(GfxBlendFactor::InvSrcAlpha)
                .setBlendOp(GfxBlendOp::Add)
                .setSrcBlendAlpha(GfxBlendFactor::One)
                .setDestBlendAlpha(GfxBlendFactor::InvSrcAlpha)
                .setBlendOpAlpha(GfxBlendOp::Add);

            GfxRasterState raster_state;
            raster_state.setCullNone().setScissorEnable(true);

            GfxRenderState render_state;
            render_state.setBlendState(blend_state);
            render_state.setDepthStencilState(depth_stencil_state);
            render_state.setRasterState(raster_state);

            auto pipeline_desc = GfxGraphicsPipelineDesc()
                .setInputLayout(m_input_layout)
                .setVertexShader(shader_library->getImGuiVertexShader())
                .setPixelShader(shader_library->getImGuiPixelShader())
                .addBindingLayout(m_binding_layout)
                .setPrimType(GfxPrimitiveType::TriangleList)
                .setRenderState(render_state);
            m_pipeline = pipeline_state_cache->resolveGraphicsPipeline(pipeline_desc, m_framebuffer->getInfo(), command_list);
        }

        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        if (!draw_data) {
            ClearOutput(command_list, output);
            return;
        }

        const int framebuffer_width = static_cast<int>(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
        const int framebuffer_height = static_cast<int>(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
        command_list.setTextureState(output, GfxAllSubresources, GfxResourceStates::RenderTarget);
        command_list.commitBarriers();
        command_list.clearTextureFloat(output, GfxAllSubresources, GfxColor(0.0f, 0.0f, 0.0f, 0.0f));

        if (framebuffer_width <= 0 || framebuffer_height <= 0 || draw_data->TotalVtxCount <= 0 || draw_data->TotalIdxCount <= 0 ||
            !m_pipeline || !m_framebuffer || !m_binding_layout)
        {
            command_list.setTextureState(output, GfxAllSubresources, GfxResourceStates::ShaderResource);
            command_list.commitBarriers();
            return;
        }

        const size_t vertex_byte_size = static_cast<size_t>(draw_data->TotalVtxCount) * sizeof(ImDrawVert);
        const size_t index_byte_size = static_cast<size_t>(draw_data->TotalIdxCount) * sizeof(ImDrawIdx);
        if (!m_vertex_buffer || static_cast<size_t>(m_vertex_buffer->getByteSize()) < vertex_byte_size) {
            m_vertex_buffer = command_list.createBuffer(
                GfxBufferDesc()
                    .setByteSize(static_cast<UInt32>((static_cast<size_t>(draw_data->TotalVtxCount) + 5000u) * sizeof(ImDrawVert)))
                    .setIsVertexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::VertexBuffer)
                    .setDebugName("ImGui Vertex Buffer"));
        }
        if (!m_index_buffer || static_cast<size_t>(m_index_buffer->getByteSize()) < index_byte_size) {
            m_index_buffer = command_list.createBuffer(
                GfxBufferDesc()
                    .setByteSize(static_cast<UInt32>((static_cast<size_t>(draw_data->TotalIdxCount) + 10000u) * sizeof(ImDrawIdx)))
                    .setIsIndexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::IndexBuffer)
                    .setDebugName("ImGui Index Buffer"));
        }
        if (!m_vertex_buffer || !m_index_buffer) {
            return;
        }

        DynamicArray<ImDrawVert> vertex_upload{};
        DynamicArray<ImDrawIdx> index_upload{};
        vertex_upload.resize(draw_data->TotalVtxCount);
        index_upload.resize(draw_data->TotalIdxCount);
        ImDrawVert* vtx_dst = vertex_upload.data();
        ImDrawIdx* idx_dst = index_upload.data();
        for (int list_index = 0; list_index < draw_data->CmdListsCount; ++list_index) {
            const ImDrawList* draw_list = draw_data->CmdLists[list_index];
            std::memcpy(vtx_dst, draw_list->VtxBuffer.Data, static_cast<size_t>(draw_list->VtxBuffer.Size) * sizeof(ImDrawVert));
            std::memcpy(idx_dst, draw_list->IdxBuffer.Data, static_cast<size_t>(draw_list->IdxBuffer.Size) * sizeof(ImDrawIdx));
            vtx_dst += draw_list->VtxBuffer.Size;
            idx_dst += draw_list->IdxBuffer.Size;
        }
        command_list.writeBuffer(m_vertex_buffer, vertex_upload.data(), vertex_byte_size);
        command_list.writeBuffer(m_index_buffer, index_upload.data(), index_byte_size);
        command_list.setBufferState(m_vertex_buffer, GfxResourceStates::VertexBuffer);
        command_list.setBufferState(m_index_buffer, GfxResourceStates::IndexBuffer);
        command_list.commitBarriers();

        GfxViewportState vp;
        vp.viewports.push_back(GfxViewport(static_cast<float>(framebuffer_width), static_cast<float>(framebuffer_height)));
        vp.scissorRects.resize(1);
        DynamicArray<GfxVertexBufferBinding> vbs;
        vbs.push_back(GfxVertexBufferBinding().setBuffer(m_vertex_buffer->getRHIHandle()).setSlot(0).setOffset(0));
        GfxIndexBufferBinding ib = GfxIndexBufferBinding()
            .setBuffer(m_index_buffer->getRHIHandle())
            .setFormat(sizeof(ImDrawIdx) == 2 ? GfxFormat::R16_UINT : GfxFormat::R32_UINT)
            .setOffset(0);

        ImGuiPushConstants push_constants{};
        push_constants.inv_display_size[0] = 1.0f / draw_data->DisplaySize.x;
        push_constants.inv_display_size[1] = 1.0f / draw_data->DisplaySize.y;
        push_constants.display_pos[0] = draw_data->DisplayPos.x;
        push_constants.display_pos[1] = draw_data->DisplayPos.y;

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
                clip_min.x = std::clamp(clip_min.x, 0.0f, static_cast<float>(framebuffer_width));
                clip_min.y = std::clamp(clip_min.y, 0.0f, static_cast<float>(framebuffer_height));
                clip_max.x = std::clamp(clip_max.x, 0.0f, static_cast<float>(framebuffer_width));
                clip_max.y = std::clamp(clip_max.y, 0.0f, static_cast<float>(framebuffer_height));
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y) {
                    continue;
                }

                auto* texture = reinterpret_cast<GfxTexture*>(draw_cmd->GetTexID());
                texture = texture ? texture : m_font_texture.get();
                if (!texture) {
                    continue;
                }
                const auto found = m_binding_sets.find(texture);
                GfxBindingSetHandle binding_set = found != m_binding_sets.end() ? found->second : nullptr;
                if (!binding_set) {
                    binding_set = command_list.createBindingSet(
                        GfxBindingSetDesc()
                            .addItem(GfxBindingSetItem::PushConstants(0, sizeof(ImGuiPushConstants)))
                            .addItem(GfxBindingSetItem::Texture_SRV(0, texture->getRHIHandle()))
                            .addItem(GfxBindingSetItem::Sampler(0, GlobalSamplers::screen())),
                        m_binding_layout);
                    if (binding_set) {
                        m_binding_sets.emplace(texture, binding_set);
                    }
                }
                if (!binding_set) {
                    continue;
                }

                vp.scissorRects[0] = GfxRect(
                    static_cast<int>(clip_min.x),
                    static_cast<int>(clip_max.x),
                    static_cast<int>(clip_min.y),
                    static_cast<int>(clip_max.y));
                DynamicArray<GfxBindingSetHandle> bs_arr = {binding_set};
                command_list.setGraphicsState(m_framebuffer, m_pipeline, bs_arr, vp, vbs, ib);
                command_list.setPushConstants(&push_constants, sizeof(push_constants));
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
#else
        (void)pass_context;
        (void)context;
        (void)command_list;
        (void)output;
#endif
    }

} // dodoe
