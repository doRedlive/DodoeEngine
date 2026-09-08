// do@Redlive

#ifdef DODOE_DEBUG_ENABLED

#include "baseline_imgui_pass.h"

#include "runtime/function/ui/imgui/imgui_builder.h"
#include "runtime/function/render/render_frame/frame_telemetry.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/render_service/input_layout_cache.h"
#include "runtime/function/render/render_settings.h"

#include <algorithm>

namespace dodoe {

    namespace {
        constexpr UInt32 kImGuiVertexCapacity = 65536;
        constexpr UInt32 kImGuiIndexCapacity = 65536;
    }

    Bool BaselineImGuiPass::initialize(const BaselinePassContext& context) {
        m_device = context.device;
        m_command_list = context.command_list;
        m_shader_library = context.shader_library;
        m_sampler = context.sampler;

        if (!m_device || !m_shader_library) {
            return true;
        }

        GfxBindingLayoutDesc layout_desc;
        layout_desc.setVisibility(GfxShaderType::Vertex | GfxShaderType::Pixel)
            .setRegisterSpaceIsDescriptorSet(true)
            .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Pass))
            .addItem(GfxBindingLayoutItem::ConstantBuffer(0))
            .addItem(GfxBindingLayoutItem::Texture_SRV(1))
            .addItem(GfxBindingLayoutItem::Sampler(9));
        m_binding_layout = m_device->createBindingLayout(layout_desc);

        if (auto* input_layout_cache = context.shared_render_service
                                            ? context.shared_render_service->getInputLayoutCache()
                                            : nullptr) {
            const DynamicArray<GfxVertexAttributeDesc> attributes = {
                GfxVertexAttributeDesc().setName("a_Position").setFormat(GfxFormat::RG32_FLOAT).setOffset(0).setElementStride(sizeof(ImDrawVert)),
                GfxVertexAttributeDesc().setName("a_UV").setFormat(GfxFormat::RG32_FLOAT).setOffset(sizeof(ImVec2)).setElementStride(sizeof(ImDrawVert)),
                GfxVertexAttributeDesc().setName("a_Color").setFormat(GfxFormat::RGBA8_UNORM).setOffset(sizeof(ImVec2) * 2).setElementStride(sizeof(ImDrawVert)),
            };
            m_input_layout = input_layout_cache->getOrCreate(attributes, m_shader_library->getImGuiVertexShader());
        }

        GfxBufferDesc vb_desc;
        vb_desc.setByteSize(static_cast<UInt64>(kImGuiVertexCapacity) * sizeof(ImDrawVert))
            .setIsVertexBuffer(true)
            .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
            .setDebugName("BaselineImGuiVB");
        m_vertex_buffer = m_device->createBuffer(vb_desc);

        GfxBufferDesc ib_desc;
        ib_desc.setByteSize(static_cast<UInt64>(kImGuiIndexCapacity) * sizeof(ImDrawIdx))
            .setIsIndexBuffer(true)
            .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
            .setDebugName("BaselineImGuiIB");
        m_index_buffer = m_device->createBuffer(ib_desc);

        GfxBufferDesc cb_desc;
        cb_desc.setByteSize(32)
            .setIsConstantBuffer(true)
            .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
            .setDebugName("BaselineImGuiCB");
        m_constant_buffer = m_device->createBuffer(cb_desc);

        return true;
    }

    void BaselineImGuiPass::shutdown() {
        m_pipeline = nullptr;
        m_binding_layout = nullptr;
        m_input_layout = nullptr;
        m_vertex_buffer = nullptr;
        m_index_buffer = nullptr;
        m_constant_buffer = nullptr;
        m_sampler = nullptr;
        m_shader_library = nullptr;
        m_command_list = nullptr;
        m_device = nullptr;
    }

    void BaselineImGuiPass::ensurePipeline(const cutie::FramebufferInfo& framebuffer_info) {
        if (m_pipeline) {
            return;
        }
        if (!m_shader_library || !m_binding_layout || !m_input_layout) {
            return;
        }
        const auto vertex_shader = m_shader_library->getImGuiVertexShader();
        const auto pixel_shader = m_shader_library->getImGuiPixelShader();
        if (!vertex_shader || !pixel_shader) {
            DO_ERROR("BaselineImGuiPass: imgui shaders are not loaded");
            return;
        }

        GfxGraphicsPipelineDesc pipeline_desc;
        pipeline_desc.setPrimType(GfxPrimitiveType::TriangleList);
        pipeline_desc.setVertexShader(vertex_shader.Get());
        pipeline_desc.setPixelShader(pixel_shader.Get());
        pipeline_desc.setInputLayout(m_input_layout.Get());
        pipeline_desc.addBindingLayout(m_binding_layout.Get());

        GfxDepthStencilState depth_stencil_state;
        depth_stencil_state.disableDepthTest().disableDepthWrite().disableStencil();
        GfxRasterState raster_state;
        raster_state.setCullNone();
        GfxBlendState blend_state;
        GfxBlendState::RenderTarget blend_target;
        blend_target.enableBlend()
            .setSrcBlend(GfxBlendFactor::SrcAlpha)
            .setDestBlend(GfxBlendFactor::OneMinusSrcAlpha)
            .setSrcBlendAlpha(GfxBlendFactor::One)
            .setDestBlendAlpha(GfxBlendFactor::OneMinusSrcAlpha);
        blend_state.setRenderTarget(0, blend_target);
        GfxRenderState render_state;
        render_state.setDepthStencilState(depth_stencil_state)
            .setRasterState(raster_state)
            .setBlendState(blend_state);
        pipeline_desc.setRenderState(render_state);

        m_pipeline = m_device->createGraphicsPipeline(pipeline_desc, framebuffer_info);
        DO_INFO("BaselineImGuiPass: render pipeline created");
    }

    void BaselineImGuiPass::render(cutie::IFramebuffer* framebuffer) {
        if (!m_pipeline || !framebuffer || !m_vertex_buffer || !m_index_buffer || !m_constant_buffer) {
            return;
        }

        const ImGuiRenderPacket& packet = ImGuiBuilder::GetRenderPacket();
        if (packet.lists.empty()) {
            return;
        }

        const auto& fb_info = framebuffer->getFramebufferInfo();
        const Float fb_size_x = static_cast<Float>(fb_info.width);
        const Float fb_size_y = static_cast<Float>(fb_info.height);
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
        if (total_vertex_count > kImGuiVertexCapacity || total_index_count > kImGuiIndexCapacity) {
            DO_ERROR("BaselineImGuiPass: transient UI buffers are too small");
            return;
        }

        m_command_list->setBufferState(m_vertex_buffer.Get(), cutie::ResourceStates::CopyDest);
        m_command_list->setBufferState(m_index_buffer.Get(), cutie::ResourceStates::CopyDest);
        m_command_list->setBufferState(m_constant_buffer.Get(), cutie::ResourceStates::CopyDest);
        RenderFrameCounters::Self().addBarrier(); m_command_list->commitBarriers();

        UInt32 vertex_offset = 0;
        UInt32 index_offset = 0;
        for (const auto& list : packet.lists) {
            if (!list.vertices.empty()) {
                m_command_list->writeBuffer(m_vertex_buffer.Get(), list.vertices.data(),
                    list.vertices.size() * sizeof(ImDrawVert), vertex_offset);
            }
            if (!list.indices.empty()) {
                m_command_list->writeBuffer(m_index_buffer.Get(), list.indices.data(),
                    list.indices.size() * sizeof(ImDrawIdx), index_offset);
            }
            vertex_offset += static_cast<UInt32>(list.vertices.size() * sizeof(ImDrawVert));
            index_offset += static_cast<UInt32>(list.indices.size() * sizeof(ImDrawIdx));
        }

        const Float left = packet.display_pos.x;
        const Float right = packet.display_pos.x + packet.display_size.x;
        const Float top = packet.display_pos.y;
        const Float bottom = packet.display_pos.y + packet.display_size.y;
        if (right <= left || bottom <= top) {
            return;
        }

        struct ImGuiPushData {
            Float inv_display_size[2];
            Float display_origin[2];
            Float ndc_y_flip;
        } push_data{
            {1.0f / (right - left), 1.0f / (bottom - top)},
            {left, top},
            RenderSettings::GetRenderBackendApiType() == RenderBackendApiType::OpenGL ? 1.0f : -1.0f};
        m_command_list->writeBuffer(m_constant_buffer.Get(), &push_data, sizeof(push_data));

        m_command_list->setBufferState(m_vertex_buffer.Get(), cutie::ResourceStates::VertexBuffer);
        m_command_list->setBufferState(m_index_buffer.Get(), cutie::ResourceStates::IndexBuffer);
        m_command_list->setBufferState(m_constant_buffer.Get(), cutie::ResourceStates::ConstantBuffer);
        RenderFrameCounters::Self().addBarrier(); m_command_list->commitBarriers();

        UInt32 global_vertex_offset = 0;
        UInt32 global_index_offset = 0;
        for (const auto& list : packet.lists) {
            for (const auto& draw : list.commands) {
                if (draw.user_callback || !draw.texture_id) {
                    continue;
                }
                auto* texture = reinterpret_cast<GfxTexture*>(draw.texture_id);
                if (!texture || !texture->isGpuReady()) {
                    continue;
                }

                auto binding_set = m_device->createBindingSet(
                    GfxBindingSetDesc()
                        .addItem(GfxBindingSetItem::ConstantBuffer(0, m_constant_buffer.Get()))
                        .addItem(GfxBindingSetItem::Texture_SRV(1, texture->getRHIHandle().Get()))
                        .addItem(GfxBindingSetItem::Sampler(9, m_sampler.Get())),
                    m_binding_layout.Get());
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

                GfxViewportState viewport_state;
                viewport_state.addViewport(GfxViewport(0.0f, fb_size_x, 0.0f, fb_size_y, 0.0f, 1.0f));
                viewport_state.addScissorRect(GfxRect(
                    static_cast<Int32>(clip_x), static_cast<Int32>(clip_z),
                    static_cast<Int32>(clip_y), static_cast<Int32>(clip_w)));

                cutie::GraphicsState graphics_state;
                graphics_state.setPipeline(m_pipeline.Get());
                graphics_state.setFramebuffer(framebuffer);
                graphics_state.setViewport(viewport_state);
                graphics_state.addBindingSet(binding_set.Get());
                graphics_state.addVertexBuffer(
                    cutie::VertexBufferBinding().setBuffer(m_vertex_buffer.Get()).setSlot(0).setOffset(global_vertex_offset));
                graphics_state.setIndexBuffer(
                    cutie::IndexBufferBinding()
                        .setBuffer(m_index_buffer.Get())
                        .setFormat(GfxFormat::R32_UINT)
                        .setOffset(global_index_offset));
                m_command_list->setGraphicsState(graphics_state);
                RenderFrameCounters::Self().addDrawCall(1);
                m_command_list->drawIndexed(GfxDrawArguments()
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
