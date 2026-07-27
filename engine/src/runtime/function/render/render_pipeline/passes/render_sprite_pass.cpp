// do@Redlive

#include "render_sprite_pass.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "../../render_view/render_view.h"
#include "../../render_view/sprite_view_extension.h"
#include "../render_pipeline_pass_utils.h"
#include "render_pass_blackboard_keys.h"

#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/render_scene/sprite_scene_info.h"
#include "runtime/function/render/gpu_driven/gpu_scene.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/shader/descriptor_table_manager.h"
#include "runtime/function/render/shader/global_samplers.h"

#include <algorithm>

namespace dodoe {

    struct SpritePassParameters {
        RenderGraphTextureHandle color_target{};
        Bool clear_target{false};
        RenderGraphBufferHandle instance_buffer{};
        RenderGraphBufferHandle quad_vertex_buffer{};
        RenderGraphBufferHandle quad_index_buffer{};
        RenderGraphBufferHandle vp_buffer{};
        DynamicArray<SpriteInstance> instances{};
    };

    void SpritePass::build(RenderGraphBuilder& graph,
                           const RenderPassBuildContext& context) {
        graph.addPass<SpritePassParameters>(
            "SpritePass",
            RenderGraphPassFlags::Raster,
            [&context](RenderGraphPassBuilder& pass_builder, SpritePassParameters& parameters) {
                const auto swapchain_extent = context.gfx_context->getSwapchainExtent2d();
                const auto* scene_color = pass_builder.blackboard().get<SceneColorKey>();
                RenderGraphAttachmentInfo color_attachment{};
                if (scene_color) {
                    color_attachment.load_op = LoadOp::Load;
                    parameters.color_target = pass_builder.writeColor(*scene_color, color_attachment);
                } else {
                    parameters.clear_target = true;
                    color_attachment.load_op = LoadOp::Clear;
                    color_attachment.clear_color = GfxColor(0.0f, 0.0f, 0.0f, 1.0f);
                    parameters.color_target = pass_builder.writeColor(pass_builder.createTransientTexture(
                        rendering_pipeline_utils::MakeSwapchainRT2D(
                            swapchain_extent, GfxFormat::RGBA8_UNORM, "RDG SpriteColor"),
                        "SpriteColor"), color_attachment);
                    pass_builder.blackboard().set<SceneColorKey>(parameters.color_target);
                }

                if (const auto* sprite_extension = context.view.getExtension<SpriteViewExtension>()) {
                    parameters.instances.reserve(sprite_extension->visible_sprites.size());
                    for (const auto* sprite : sprite_extension->visible_sprites) {
                        if (sprite) {
                            parameters.instances.push_back(sprite->toInstance());
                        }
                    }
                }

                RenderGraphBufferDesc instance_desc{};
                instance_desc.desc = GfxBufferDesc()
                    .setByteSize(static_cast<UInt32>(std::max<Size_t>(parameters.instances.size(), 1) * sizeof(SpriteInstance)))
                    .setIsVertexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
                    .setDebugName("RDG VisibleSpriteInstances");
                parameters.instance_buffer = pass_builder.writeBuffer(
                    pass_builder.createTransientBuffer(instance_desc, "VisibleSpriteInstances"),
                    RenderGraphPipelineStage::Copy);

                RenderGraphBufferDesc vp_desc{};
                vp_desc.desc = GfxBufferDesc()
                    .setByteSize(sizeof(Matrix4f))
                    .setIsConstantBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
                    .setDebugName("RDG SpriteVP");
                parameters.vp_buffer = pass_builder.writeBuffer(
                    pass_builder.createTransientBuffer(vp_desc, "SpriteVpBuffer"),
                    RenderGraphPipelineStage::Copy);

                const auto* gpu_scene = context.scene ? context.scene->getGpuScene() : nullptr;
                if (!gpu_scene) {
                    return;
                }
                const auto scene_resources = gpu_scene->getPassResources();
                if (scene_resources.quad_vb && scene_resources.quad_ib) {
                    parameters.quad_vertex_buffer = pass_builder.importBuffer(scene_resources.quad_vb, "SpriteQuadVB");
                    parameters.quad_index_buffer = pass_builder.importBuffer(scene_resources.quad_ib, "SpriteQuadIB");
                }
            },
            [this](const SpritePassParameters& parameters, const RenderGraphPassContext& ctx,
                   DrawCommandList& command_list) {
                if (parameters.instances.empty() || !parameters.quad_vertex_buffer.isValid() ||
                    !parameters.quad_index_buffer.isValid() || !m_binding_layout) {
                    return;
                }

                auto* shared_service = ctx.getSharedRenderService();
                const auto* shader_library = ctx.getShaderLibrary();
                const auto* pipeline_cache = ctx.getPipelineStateCache();
                const auto* descriptor_manager = shared_service ? shared_service->getDescriptorTable() : nullptr;
                if (!shader_library || !pipeline_cache || !descriptor_manager ||
                    !descriptor_manager->getDescriptorTable() || !m_input_layout) {
                    DO_ERROR("SpritePass: bindless sprite resources are unavailable");
                    return;
                }

                const auto instance_buffer = ctx.resolveBuffer(parameters.instance_buffer);
                const auto vp_buffer = ctx.resolveBuffer(parameters.vp_buffer);
                const auto quad_vertex_buffer = ctx.resolveBuffer(parameters.quad_vertex_buffer);
                const auto quad_index_buffer = ctx.resolveBuffer(parameters.quad_index_buffer);

                command_list.setBufferState(instance_buffer, GfxResourceStates::CopyDest);
                command_list.setBufferState(vp_buffer, GfxResourceStates::CopyDest);
                command_list.commitBarriers();
                command_list.writeBuffer(instance_buffer, parameters.instances.data(),
                                         parameters.instances.size() * sizeof(SpriteInstance));
                const Matrix4f view_projection = ctx.getView()->getViewProjectionMatrix();
                command_list.writeBuffer(vp_buffer, &view_projection, sizeof(view_projection));
                command_list.setBufferState(instance_buffer, GfxResourceStates::VertexBuffer);
                command_list.setBufferState(vp_buffer, GfxResourceStates::ConstantBuffer);
                command_list.setBufferState(quad_vertex_buffer, GfxResourceStates::VertexBuffer);
                command_list.setBufferState(quad_index_buffer, GfxResourceStates::IndexBuffer);
                command_list.commitBarriers();

                const auto binding_set = command_list.createBindingSet(
                    GfxBindingSetDesc()
                        .addItem(GfxBindingSetItem::ConstantBuffer(0, vp_buffer->getRHI()))
                        .addItem(GfxBindingSetItem::Sampler(0, GlobalSamplers::screen().Get())),
                    m_binding_layout);
                if (!binding_set) {
                    DO_ERROR("SpritePass: failed to create binding set");
                    return;
                }

                const auto descriptor_binding_set = create_ref<GfxBindingSet>(
                    cutie::BindingSetHandle(descriptor_manager->getDescriptorTable()));
                GfxGraphicsPipelineDesc pipeline_desc = GfxGraphicsPipelineDesc()
                    .setVertexShader(shader_library->getSpriteVertexShader())
                    .setPixelShader(shader_library->getSpritePixelShader())
                    .setInputLayout(m_input_layout)
                    .addBindingLayout(m_binding_layout)
                    .addBindingLayout(descriptor_manager->getDescriptorTable()->getLayout())
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
                    DO_ERROR("SpritePass: failed to create pipeline");
                    return;
                }

                DynamicArray<GfxBindingSetHandle> binding_sets = {binding_set, descriptor_binding_set};
                DynamicArray<GfxVertexBufferBinding> vertex_buffers = {
                    GfxVertexBufferBinding().setBuffer(quad_vertex_buffer->getRHIHandle()).setSlot(0).setOffset(0),
                    GfxVertexBufferBinding().setBuffer(instance_buffer->getRHIHandle()).setSlot(1).setOffset(0)};
                command_list.setGraphicsState(
                    ctx.getFramebuffer(), pipeline, binding_sets,
                    rendering_pipeline_utils::BuildViewportState(*ctx.getView(), ctx.getGfxContext()->getSwapchainExtent2d()),
                    vertex_buffers,
                    GfxIndexBufferBinding().setBuffer(quad_index_buffer->getRHIHandle()));
                command_list.drawIndexed(GfxDrawArguments()
                    .setVertexCount(6)
                    .setInstanceCount(static_cast<UInt32>(parameters.instances.size())));
            });
    }

} // namespace dodoe
