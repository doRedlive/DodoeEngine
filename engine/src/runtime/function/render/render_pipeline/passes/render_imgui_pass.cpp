// do@Redlive

#include "render_imgui_pass.h"

#include "render_pass_blackboard_keys.h"

#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_pipeline/render_graph_import_keys.h"
#include "runtime/function/render/render_pipeline/render_graph_import_registry.h"
#include "runtime/function/render/render_service/shared_render_service.h"

#if defined(DODOE_DEBUG_ENABLED) && defined(DODOE_IMGUI_ENABLED)
#include "imgui/imgui.h"
#include "runtime/function/ui/imgui/imgui_builder.h"

namespace dodoe {

    struct ImGuiPassParameters {
        RenderGraphTextureHandle output{};
        RenderGraphTextureHandle font_texture{};
        RenderGraphBufferHandle imgui_cb{};
        RenderGraphBufferHandle vertex_buffer{};
        RenderGraphBufferHandle index_buffer{};
        ImGuiRenderPacket packet{};
    };

    void ImGuiPass::build(RenderGraphBuilder& graph,
                          const RenderPassBuildContext& context) {
        graph.addPass<ImGuiPassParameters>(
            "ImGuiPass",
            RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
            [&context](RenderGraphPassBuilder& pass_builder, ImGuiPassParameters& parameters) {
                const auto swapchain_extent = context.gfx_context->getSwapchainExtent2D();
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
                RenderGraphBufferDesc cb_desc{};
                cb_desc.desc.setByteSize(sizeof(ImGuiViewportConstants)).setIsConstantBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::CopyDest).setDebugName("ImGuiViewportCB");
                parameters.imgui_cb = pass_builder.createTransientBuffer(cb_desc, "ImGuiViewportCB");

                parameters.packet = ImGuiBuilder::GetRenderPacket();
                const auto& packet = parameters.packet;
                Size_t vertex_count = 1;
                Size_t index_count = 1;
                for (const auto& list : packet.lists) {
                    vertex_count += list.vertices.size();
                    index_count += list.indices.size();
                }

                RenderGraphBufferDesc vb_desc{};
                vb_desc.desc = GfxBufferDesc()
                    .setByteSize(static_cast<UInt64>(vertex_count) * sizeof(ImDrawVert))
                    .setIsVertexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
                    .setDebugName("RDG ImGuiVB");
                parameters.vertex_buffer = pass_builder.createTransientBuffer(vb_desc, "ImGuiVertexBuffer");

                RenderGraphBufferDesc ib_desc{};
                ib_desc.desc = GfxBufferDesc()
                    .setByteSize(static_cast<UInt64>(index_count) * sizeof(ImDrawIdx))
                    .setIsIndexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
                    .setDebugName("RDG ImGuiIB");
                parameters.index_buffer = pass_builder.createTransientBuffer(ib_desc, "ImGuiIndexBuffer");
                UInt64 vertex_offset = 0, index_offset = 0;
                UnorderedSet<GfxTexture*> textures;
                for (const auto& list : packet.lists) {
                    const Size_t vb_bytes = list.vertices.size() * sizeof(ImDrawVert);
                    const Size_t ib_bytes = list.indices.size() * sizeof(ImDrawIdx);
                    pass_builder.uploadBuffer(parameters.vertex_buffer, list.vertices.data(), vb_bytes, vertex_offset);
                    pass_builder.uploadBuffer(parameters.index_buffer, list.indices.data(), ib_bytes, index_offset);
                    vertex_offset += vb_bytes;
                    index_offset += ib_bytes;
                    for (const auto& draw : list.commands) {
                        const auto& texture = draw.texture;
                        if (!draw.user_callback && texture && textures.insert(texture.get()).second)
                            pass_builder.read(pass_builder.importTexture(texture, "ImGuiTexture"));
                    }
                }
                const auto constants = ImGuiDrawRenderer::makeViewportConstants(packet);
                pass_builder.uploadBuffer(parameters.imgui_cb, &constants, sizeof(constants));
                pass_builder.readBuffer(parameters.imgui_cb, RenderGraphBufferUsage::Constant);
                if (vertex_offset) pass_builder.readBuffer(parameters.vertex_buffer, RenderGraphBufferUsage::Vertex);
                if (index_offset) pass_builder.readBuffer(parameters.index_buffer, RenderGraphBufferUsage::Index);
            },
            [this](const ImGuiPassParameters& parameters, const RenderGraphPassContext& ctx,
               DrawCommandList& command_list) {
                const auto& packet = parameters.packet;
                if (packet.lists.empty()) {
                    return;
                }

                const auto vb = ctx.resolveBuffer(parameters.vertex_buffer);
                const auto ib = ctx.resolveBuffer(parameters.index_buffer);
                const auto imgui_cb = ctx.resolveBuffer(parameters.imgui_cb);
                const auto framebuffer = ctx.getFramebuffer();
                m_renderer.render(packet, framebuffer, ctx.getRenderTargetSignature(),
                                  vb, ib, imgui_cb, command_list,
                                  ctx.getPipelineStateCache(), ctx.getShaderLibrary(), true);
            });
    }

} // namespace dodoe

#endif
