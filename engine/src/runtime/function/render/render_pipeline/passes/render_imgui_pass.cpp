// do@Redlive

#include "render_imgui_pass.h"

#include "render_pass_blackboard_keys.h"

#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_pipeline/render_graph_import_keys.h"
#include "runtime/function/render/render_pipeline/render_graph_import_registry.h"
#include "runtime/function/render/render_service/shared_render_service.h"

#ifdef DODOE_DEBUG_ENABLED
#include "imgui/imgui.h"
#include "runtime/function/ui/imgui/imgui_builder.h"

namespace dodoe {

    struct ImGuiPassParameters {
        RenderGraphTextureHandle output{};
        RenderGraphTextureHandle font_texture{};
        RenderGraphBufferHandle imgui_cb{};
        RenderGraphBufferHandle vertex_buffer{};
        RenderGraphBufferHandle index_buffer{};
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
                parameters.imgui_cb = pass_builder.write(pass_builder.importBuffer(
                    context.graph_imports->require<ImGuiConstantBufferKey>(), "ImGuiViewportCB"));

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
                const auto& packet = ImGuiBuilder::GetRenderPacket();
                if (packet.lists.empty()) {
                    return;
                }

                const auto vb = ctx.resolveBuffer(parameters.vertex_buffer);
                const auto ib = ctx.resolveBuffer(parameters.index_buffer);
                const auto imgui_cb = ctx.resolveBuffer(parameters.imgui_cb);
                const auto framebuffer = ctx.getFramebuffer();
                m_renderer.render(packet, framebuffer, ctx.getRenderTargetSignature(),
                                  vb, ib, imgui_cb, command_list,
                                  ctx.getPipelineStateCache(), ctx.getShaderLibrary());
            });
    }

} // namespace dodoe

#endif
