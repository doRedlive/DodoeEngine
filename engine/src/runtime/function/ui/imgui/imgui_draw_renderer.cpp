// do@Redlive

#ifdef DODOE_DEBUG_ENABLED

#include "imgui_draw_renderer.h"

#include "imgui_builder.h"

#include "runtime/function/render/pipeline_state/pipeline_state_cache.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/shader/global_samplers.h"

#include "imgui/imgui.h"

#include <algorithm>

namespace dodoe {

    ImGuiDrawRenderer::ImGuiDrawRenderer(GfxBindingLayoutHandle binding_layout,
                                         GfxBindingSetHandle font_binding_set,
                                         GfxInputLayoutHandle input_layout,
                                         GfxTextureHandle font_texture,
                                         GfxBufferHandle font_binding_set_cb)
        : m_binding_layout(std::move(binding_layout))
        , m_font_binding_set(std::move(font_binding_set))
        , m_input_layout(std::move(input_layout))
        , m_font_texture(std::move(font_texture))
        , m_font_binding_set_cb(std::move(font_binding_set_cb)) {
    }

    void ImGuiDrawRenderer::render(const ImGuiRenderPacket& packet,
                                   const GfxFramebufferHandle& framebuffer,
                                   const GfxFramebufferInfo& render_target_signature,
                                   const GfxBufferHandle& vertex_buffer,
                                   const GfxBufferHandle& index_buffer,
                                   const GfxBufferHandle& constant_buffer,
                                   DrawCommandList& command_list,
                                   const PipelineStateCache* pipeline_cache,
                                   const ShaderLibrary* shader_library) {
        if (packet.lists.empty()) {
            return;
        }

        const UInt32 fb_width = framebuffer && framebuffer->getRHI()
            ? framebuffer->getRHI()->getFramebufferInfo().width : 0u;
        const UInt32 fb_height = framebuffer && framebuffer->getRHI()
            ? framebuffer->getRHI()->getFramebufferInfo().height : 0u;
        const Float fb_size_x = static_cast<Float>(fb_width);
        const Float fb_size_y = static_cast<Float>(fb_height);
        if (fb_size_x <= 0.0f || fb_size_y <= 0.0f) {
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

        const auto& vb = vertex_buffer;
        const auto& ib = index_buffer;
        const UInt64 vertex_bytes = static_cast<UInt64>(total_vertex_count) * sizeof(ImDrawVert);
        const UInt64 index_bytes = static_cast<UInt64>(total_index_count) * sizeof(ImDrawIdx);
        if (vertex_bytes > vb->getByteSize() || index_bytes > ib->getByteSize()) {
            DO_ERROR("ImGuiDrawRenderer: transient UI buffers are too small");
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

        if (!shader_library || !pipeline_cache || !m_binding_layout || !m_input_layout) {
            DO_ERROR("ImGuiDrawRenderer: shader or pipeline resources are unavailable");
            return;
        }

        auto pipeline_desc = GfxGraphicsPipelineDesc()
            .setVertexShader(shader_library->getImGuiVertexShader())
            .setPixelShader(shader_library->getImGuiPixelShader())
            .setInputLayout(m_input_layout)
            .addBindingLayout(m_binding_layout)
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
            pipeline_desc, render_target_signature, command_list);
        if (!pipeline) {
            DO_ERROR("ImGuiDrawRenderer: failed to create pipeline");
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

        command_list.setBufferState(constant_buffer, GfxResourceStates::CopyDest);
        command_list.commitBarriers();
        command_list.writeBuffer(constant_buffer, &push_data, sizeof(push_data));
        command_list.setBufferState(constant_buffer, GfxResourceStates::ConstantBuffer);

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
                if (!texture || !texture->isGpuReady()) {
                    continue;
                }
                const auto binding_set = texture == m_font_texture.get() && m_font_binding_set &&
                                         constant_buffer.get() == m_font_binding_set_cb.get()
                    ? m_font_binding_set
                    : command_list.createBindingSet(
                        GfxBindingSetDesc()
                            .addItem(GfxBindingSetItem::ConstantBuffer(0, constant_buffer->getRHIHandle().Get()))
                            .addItem(GfxBindingSetItem::Texture_SRV(1, texture->getRHI()))
                            .addItem(GfxBindingSetItem::Sampler(9, GlobalSamplers::screen().Get())),
                        m_binding_layout);
                if (!binding_set) {
                    continue;
                }

                const Float clip_x = std::clamp(draw.clip_rect.x - packet.display_pos.x, 0.0f, fb_size_x);
                const Float clip_y = std::clamp(draw.clip_rect.y - packet.display_pos.y, 0.0f, fb_size_y);
                const Float clip_z = std::clamp(draw.clip_rect.z - packet.display_pos.x, 0.0f, fb_size_x);
                const Float clip_w = std::clamp(draw.clip_rect.w - packet.display_pos.y, 0.0f, fb_size_y);
                if (clip_z <= clip_x || clip_w <= clip_y) {
                    continue;
                }

                GfxViewportState viewport;
                viewport.addViewport(GfxViewport(0.0f, fb_size_x, 0.0f, fb_size_y, 0.0f, 1.0f));
                viewport.addScissorRect(GfxRect(
                    static_cast<Int32>(clip_x), static_cast<Int32>(clip_z),
                    static_cast<Int32>(clip_y), static_cast<Int32>(clip_w)));

                DynamicArray<GfxBindingSetHandle> binding_sets = {binding_set};
                DynamicArray<GfxVertexBufferBinding> vertex_buffers = {
                    GfxVertexBufferBinding().setBuffer(vb->getRHIHandle()).setSlot(0).setOffset(global_vertex_offset)};
                command_list.setGraphicsState(framebuffer, pipeline, binding_sets, viewport, vertex_buffers,
                    GfxIndexBufferBinding().setBuffer(ib->getRHIHandle()).setFormat(GfxFormat::R16_UINT).setOffset(global_index_offset));
                command_list.drawIndexed(GfxDrawArguments()
                    .setVertexCount(draw.elem_count)
                    .setStartIndexLocation(draw.idx_offset)
                    .setStartVertexLocation(draw.vtx_offset));
            }
            global_vertex_offset += static_cast<UInt32>(list.vertices.size() * sizeof(ImDrawVert));
            global_index_offset += static_cast<UInt32>(list.indices.size() * sizeof(ImDrawIdx));
        }
    }

} // namespace dodoe

#endif // DODOE_DEBUG_ENABLED
