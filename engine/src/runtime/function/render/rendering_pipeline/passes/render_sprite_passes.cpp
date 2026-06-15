// do@Redlive

#include "render_sprite_passes.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "../render_view.h"
#include "../rendering_pipeline_pass_utils.h"

#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/framework/sprite_scene_info.h"
#include "runtime/function/render/renderer.h"

namespace dodoe::RenderingPipelinePasses {
    namespace {

        struct SpritePassParameters {
            RenderGraphTextureHandle color_target{};
            RenderGraphBufferHandle instance_buffer{};
            RenderGraphBufferHandle quad_vertex_buffer{};
            RenderGraphBufferHandle quad_index_buffer{};
        };

        void addSpritePass(RenderGraphBuilder& graph, const RenderingPassContext& pass_context) {
            graph.addPass<SpritePassParameters>(
                "MainSpritePass",
                RenderGraphPassFlags::Raster,
                [pass_context](RenderGraphPassBuilder& pass_builder, SpritePassParameters& parameters) {
                    const auto swapchain_extent = pass_context.gfx_context->getSwapchainExtent2d();

                    RenderGraphTextureDesc color_desc{};
                    color_desc.desc = GfxTextureDesc()
                        .setDimension(GfxTextureDimension::Texture2D)
                        .setWidth(static_cast<UInt32>(swapchain_extent.x))
                        .setHeight(static_cast<UInt32>(swapchain_extent.y))
                        .setFormat(GfxFormat::RGBA8_UNORM)
                        .setIsRenderTarget(true)
                        .setDebugName("RDG SpriteColor");

                    parameters.color_target = pass_builder.write(pass_builder.createTransientTexture(color_desc, "SpriteColor"));

                    const auto& scene = Renderer::GetRenderScene();
                    const auto& sprite_info = scene.getSpriteSceneInfo();
                    const UInt32 instance_count = sprite_info.m_instance_count > 0 ? sprite_info.m_instance_count : 1;

                    RenderGraphBufferDesc instance_buffer_desc{};
                    instance_buffer_desc.desc = GfxBufferDesc()
                        .setByteSize(instance_count * static_cast<UInt32>(sizeof(SpriteInstance)))
                        .setIsVertexBuffer(true)
                        .setDebugName("RDG SpriteInstanceBuffer");
                    parameters.instance_buffer = pass_builder.write(pass_builder.createTransientBuffer(instance_buffer_desc, "SpriteInstanceBuffer"));

                    RenderGraphBufferDesc quad_vb_desc{};
                    quad_vb_desc.desc = GfxBufferDesc()
                        .setByteSize(static_cast<UInt32>(sizeof(kQuadVertices)))
                        .setIsVertexBuffer(true)
                        .setDebugName("RDG SpriteQuadVB");
                    parameters.quad_vertex_buffer = pass_builder.write(pass_builder.createTransientBuffer(quad_vb_desc, "SpriteQuadVB"));

                    RenderGraphBufferDesc quad_ib_desc{};
                    quad_ib_desc.desc = GfxBufferDesc()
                        .setByteSize(static_cast<UInt32>(sizeof(kQuadIndices)))
                        .setIsIndexBuffer(true)
                        .setDebugName("RDG SpriteQuadIB");
                    parameters.quad_index_buffer = pass_builder.write(pass_builder.createTransientBuffer(quad_ib_desc, "SpriteQuadIB"));
                },
                [](const SpritePassParameters& parameters, const RenderGraphPassContext& context, RenderGraphCommandList& command_list) {
                    const auto& scene = Renderer::GetRenderScene();
                    const auto& sprite_info = scene.getSpriteSceneInfo();

                    if (sprite_info.m_instance_count == 0) {
                        return;
                    }

                    const auto device = context.getGfxContext()->getDevice();
                    const auto color_target = command_list.resolveTexture(parameters.color_target);
                    auto framebuffer = device->createFramebuffer(
                        GfxFramebufferDesc().addColorAttachment(color_target)
                    );

                    command_list.open();
                    command_list.beginMarker("MainSpritePass");

                    command_list.setTextureState(parameters.color_target, GfxAllSubresources, GfxResourceStates::RenderTarget);
                    command_list.commitBarriers();
                    command_list.clearTextureFloat(parameters.color_target, GfxAllSubresources, GfxColor(0.0f, 0.0f, 0.0f, 0.0f));

                    command_list.setBufferState(parameters.instance_buffer, GfxResourceStates::CopyDest);
                    command_list.commitBarriers();
                    command_list.writeBuffer(parameters.instance_buffer, sprite_info.m_instances.data(), sprite_info.m_instance_count * sizeof(SpriteInstance));

                    command_list.endMarker();
                    command_list.close();
                }
            );
        }

    } // namespace

    void AddSpriteGraphPasses(RenderGraphBuilder& graph, const RenderingPassContext& pass_context) {
        addSpritePass(graph, pass_context);
    }

} // namespace dodoe::RenderingPipelinePasses
