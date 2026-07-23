// do@Redlive

#include "render_sprite_pass.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "../../render_view/render_view.h"
#include "../../render_view/sprite_view_extension.h"
#include "../render_pipeline_pass_utils.h"

#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/gpu_driven/gpu_scene.h"
#include "runtime/function/render/gpu_driven/gpu_driven_renderer.h"
#include "runtime/function/render/shader/global_samplers.h"
#include "runtime/function/render/mesh_draw/local_vertex_factory.h"
#include "runtime/function/render/texture/texture_manager.h"
#include "runtime/function/render/shader/descriptor_table_manager.h"
#include "runtime/function/render/render_settings.h"
#include "runtime/core/utils/common.h"
#include "runtime/core/math/math.h"
#include "render_pass_blackboard_keys.h"

namespace dodoe {

    struct SpritePassParameters {
        RenderGraphTextureHandle color_target{};
        RenderGraphBufferHandle instance_buffer{};
        RenderGraphBufferHandle quad_vertex_buffer{};
        RenderGraphBufferHandle quad_index_buffer{};
        RenderGraphBufferHandle vp_buffer{};
        RenderGraphBufferHandle indirect_args{};
    };

    void SpritePass::build(RenderGraphBuilder& graph,
                            const RenderPassBuildContext& context) {
        const auto& pass_context = context.pass_context;
        DO_ASSERT(pass_context.isValid(), "RenderingPipeline pass context is invalid");

        graph.addPass<SpritePassParameters>(
            "SpritePass",
            RenderGraphPassFlags::Raster,
            [pass_context](RenderGraphPassBuilder& pass_builder, SpritePassParameters& parameters) {
                const auto swapchain_extent = pass_context.gfx_context->getSwapchainExtent2d();
                const auto* scene_color = pass_builder.blackboard().get<SceneColorKey, RenderGraphTextureHandle>();
                if (scene_color) {
                    parameters.color_target = pass_builder.write(*scene_color);
                } else {
                    parameters.color_target = pass_builder.write(pass_builder.createTransientTexture(
                        rendering_pipeline_utils::MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA8_UNORM, "RDG SpriteColor"),
                        "SpriteColor"));
                    pass_builder.blackboard().set<SceneColorKey>(parameters.color_target);
                }

                RenderGraphBufferDesc vp_desc{};
                vp_desc.desc = GfxBufferDesc()
                    .setByteSize(sizeof(Matrix4f))
                    .setIsConstantBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::ConstantBuffer)
                    .setDebugName("RDG SpriteVP");
                parameters.vp_buffer = pass_builder.write(pass_builder.createTransientBuffer(vp_desc, "SpriteVpBuffer"));
            },
            [pass_context](const SpritePassParameters& parameters, const RenderGraphPassContext& ctx, DrawCommandList& command_list) {
                if (!parameters.instance_buffer.isValid() || !parameters.indirect_args.isValid()) {
                    return;
                }

                const auto* view = ctx.getView();
                const auto* sprite_ext = view->getExtension<SpriteViewExtension>();
                if (!sprite_ext || sprite_ext->visible_count == 0) {
                    return;
                }

                const auto color_target = ctx.resolveTexture(parameters.color_target);
                const auto vp_buffer = ctx.resolveBuffer(parameters.vp_buffer);

                Matrix4f vp_matrix = view->getViewProjectionMatrix();
                command_list.setBufferState(vp_buffer, GfxResourceStates::CopyDest);
                command_list.commitBarriers();
                command_list.writeBuffer(vp_buffer, &vp_matrix, sizeof(Matrix4f));
                command_list.setBufferState(vp_buffer, GfxResourceStates::ConstantBuffer);
                command_list.commitBarriers();

                auto framebuffer_desc = GfxFramebufferDesc().addColorAttachment(color_target);
                auto framebuffer = command_list.createFramebuffer(framebuffer_desc);

                const auto viewport_state = rendering_pipeline_utils::BuildViewportState(*ctx.getView(), ctx.getGfxContext()->getSwapchainExtent2d());

                const auto* descriptor_table = pass_context.getSharedRenderService()->getDescriptorTable();
                if (!descriptor_table || !descriptor_table->getDescriptorTable()) {
                    return;
                }
                const auto* texture_manager = pass_context.getTextureManager();
                const auto* sampler = texture_manager ? texture_manager->getScreenSampler() : GfxSamplerHandle{};
                if (!sampler) {
                    sampler = command_list.createSampler(GfxSamplerDesc());
                }

                auto sprite_ps = pass_context.getShaderLibrary()->getSpritePixelShader();
                auto sprite_vs = pass_context.getShaderLibrary()->getSpriteVertexShader();
                auto vertex_factory = pass_context.getSharedRenderService()->getInputLayoutCache()
                    ? pass_context.getSharedRenderService()->getInputLayoutCache()->find("Sprite")
                    : GfxInputLayoutHandle{};

                auto binding_layout = command_list.createBindingLayout(
                    GfxBindingLayoutDesc()
                        .setVisibility(GfxShaderType::Vertex | GfxShaderType::Pixel)
                        .addItem(GfxBindingLayoutItem::ConstantBuffer(0, GfxShaderType::Vertex))
                        .addItem(GfxBindingLayoutItem::Texture_SRV_Array(0, GfxShaderType::Pixel))
                        .addItem(GfxBindingLayoutItem::Sampler(0, GfxShaderType::Pixel)));

                auto gfx_vp_buf = ctx.resolveBuffer(parameters.vp_buffer);
                auto binding_set = command_list.createBindingSet(
                    GfxBindingSetDesc()
                        .addItem(GfxBindingSetItem::ConstantBuffer_VS(0, gfx_vp_buf->getRHIHandle()))
                        .addItem(GfxBindingSetItem::Texture_SRV_Array(0, texture_manager
                            ? texture_manager->getTextureArrayRHI()
                            : GfxTextureHandle{}, 0, 1))
                        .addItem(GfxBindingSetItem::Sampler(0, sampler)),
                    binding_layout);

                DynamicArray<GfxBindingSetHandle> bs_arr = {binding_set};
                GfxGraphicsPipelineDesc pipeline_desc = GfxGraphicsPipelineDesc()
                    .setVertexShader(sprite_vs)
                    .setPixelShader(sprite_ps)
                    .addBindingLayout(binding_layout)
                    .setPrimType(GfxPrimitiveType::TriangleList);
                if (vertex_factory) {
                    pipeline_desc.setInputLayout(vertex_factory);
                }
                GfxDepthStencilState depth_stencil;
                depth_stencil.enableDepthTest().setDepthFunc(GfxComparisonFunc::Less).disableDepthWrite().disableStencil();
                GfxRasterState raster;
                raster.setCullNone();
                GfxRenderState render_state;
                render_state.setDepthStencilState(depth_stencil).setRasterState(raster);
                pipeline_desc.setRenderState(render_state);

                GfxFramebufferInfo framebuffer_info(framebuffer_desc);
                auto pipeline = pass_context.getPipelineStateCache()->resolveGraphicsPipeline(pipeline_desc, framebuffer_info, command_list);
                if (!pipeline) {
                    DO_ERROR("SpritePass: Failed to create pipeline");
                    return;
                }

                command_list.setTextureState(color_target, GfxAllSubresources, GfxResourceStates::RenderTarget);
                command_list.commitBarriers();

                DynamicArray<GfxVertexBufferBinding> vbs;
                vbs.push_back(GfxVertexBufferBinding()
                    .setBuffer(ctx.resolveBuffer(parameters.quad_vertex_buffer)->getRHIHandle()).setSlot(0).setOffset(0));
                vbs.push_back(GfxVertexBufferBinding()
                    .setBuffer(ctx.resolveBuffer(parameters.instance_buffer)->getRHIHandle()).setSlot(1).setOffset(0));
                command_list.setIndexBuffer(GfxIndexBufferBinding()
                    .setBuffer(ctx.resolveBuffer(parameters.quad_index_buffer)->getRHIHandle()));

                command_list.setBufferState(ctx.resolveBuffer(parameters.indirect_args), GfxResourceStates::IndirectArgument);
                command_list.commitBarriers();

                command_list.setGraphicsState(framebuffer, pipeline, bs_arr, viewport_state, vbs);
                command_list.drawIndexedIndirect(0, sprite_ext->visible_count);

                command_list.setTextureState(color_target, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.commitBarriers();
            }
        );
    }

} // namespace dodoe
