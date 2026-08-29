// do@Redlive

#include "render_sprite_pass.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "runtime/function/render/render_view/render_view.h"
#include "runtime/function/render/render_view/sprite_view_extension.h"
#include "runtime/function/render/render_pipeline/render_pipeline_pass_utils.h"
#include "render_pass_blackboard_keys.h"

#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/render_scene/sprite_scene_info.h"
#include "runtime/function/render/gpu_driven/gpu_scene.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/shader/descriptor_table_manager.h"
#include "runtime/function/render/shader/global_samplers.h"
#include "runtime/function/render/render_settings.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/pipeline_state/pipeline_state_cache.h"

namespace dodoe {

    struct SpritePassParameters {
        RenderGraphTextureHandle color_target{};
        Bool clear_target{false};
        Bool depth_occlusion{false};
        RenderGraphBufferHandle instance_buffer{};
        RenderGraphBufferHandle quad_vertex_buffer{};
        RenderGraphBufferHandle quad_index_buffer{};
        RenderGraphBufferHandle vp_buffer{};
        FrameArray<SpriteInstance> instances{};
    };

    void SpritePass::build(RenderGraphBuilder& graph,
                           const RenderPassBuildContext& context) {
        graph.addPass<SpritePassParameters>(
            "SpritePass",
            RenderGraphPassFlags::Raster,
            [&context](RenderGraphPassBuilder& pass_builder, SpritePassParameters& parameters) {
                const auto swapchain_extent = context.gfx_context->getSwapchainExtent2D();
                const auto* hdr = pass_builder.blackboard().get<SceneHdrKey>();
                const auto* scene_color = pass_builder.blackboard().get<SceneColorKey>();
                RenderGraphAttachmentInfo color_attachment{};
                if (hdr) {
                    color_attachment.load_op = LoadOp::Load;
                    parameters.color_target = pass_builder.writeColor(*hdr, color_attachment);
                } else if (scene_color) {
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

                if (const auto* scene_textures = pass_builder.blackboard().get<SceneTexturesKey>()) {
                    if (scene_textures->depth.isValid()) {
                        RenderGraphAttachmentInfo depth_attachment{};
                        depth_attachment.load_op = LoadOp::Load;
                        pass_builder.writeDepth(scene_textures->depth, depth_attachment);
                        parameters.depth_occlusion = true;
                    }
                }

                if (const auto* sprite_extension = context.view.getExtension<SpriteViewExtension>()) {
                    auto* texture_manager = context.scene ? context.scene->getTextureManager() : nullptr;
                    Size_t instance_count = 0;
                    for (const auto* sprite : sprite_extension->visible_sprites) {
                        if (!sprite) continue;
                        instance_count += sprite->hasInstances() ? sprite->getInstances().size() : 1;
                    }
                    parameters.instances.reserve(instance_count);
                    for (const auto* sprite : sprite_extension->visible_sprites) {
                        if (!sprite) continue;
                        if (sprite->hasInstances()) {
                            const auto& atlases = sprite->getBatchAtlases();
                            for (const auto& base : sprite->getInstances()) {
                                SpriteInstance instance = base;
                                const Size_t atlas = instance.atlas_index;
                                const auto* texture = atlas < atlases.size() ? atlases[atlas].get() : nullptr;
                                instance.atlas_index = texture_manager
                                    ? texture_manager->resolveAtlasIndex(texture)
                                    : 0;
                                parameters.instances.push_back(instance);
                            }
                        } else {
                            SpriteInstance instance = sprite->toInstance();
                            instance.atlas_index = context.scene ? context.scene->resolveSpriteAtlasIndex(*sprite) : 0;
                            parameters.instances.push_back(instance);
                        }
                    }
                    // 层级排序：sorting_key 升序（稳定），同 key 内按 atlas 聚合；
                    // 传统路径依赖 atlas 连续分段绘制，绑定无路径则仅受 sorting_key 影响
                    std::stable_sort(parameters.instances.begin(), parameters.instances.end(),
                        [](const SpriteInstance& a, const SpriteInstance& b) {
                            if (a.sorting_key != b.sorting_key) {
                                return a.sorting_key < b.sorting_key;
                            }
                            return a.atlas_index < b.atlas_index;
                        });
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
                pass_builder.readBuffer(parameters.instance_buffer, RenderGraphPipelineStage::VertexShader);

                RenderGraphBufferDesc vp_desc{};
                vp_desc.desc = GfxBufferDesc()
                    .setByteSize(sizeof(Matrix4f))
                    .setIsConstantBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
                    .setDebugName("RDG SpriteVP");
                parameters.vp_buffer = pass_builder.writeBuffer(
                    pass_builder.createTransientBuffer(vp_desc, "SpriteVpBuffer"),
                    RenderGraphPipelineStage::Copy);
                pass_builder.readBuffer(parameters.vp_buffer, RenderGraphPipelineStage::VertexShader);

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
                const auto color_target = ctx.resolveTexture(parameters.color_target);
                command_list.setTextureState(color_target, GfxAllSubresources, GfxResourceStates::RenderTarget);
                command_list.commitBarriers();
                if (parameters.clear_target) {
                    command_list.clearTextureFloat(color_target, GfxAllSubresources, GfxColor(0.0f, 0.0f, 0.0f, 1.0f));
                }

                if (parameters.instances.empty() || !parameters.quad_vertex_buffer.isValid() ||
                    !parameters.quad_index_buffer.isValid() || !m_input_layout) {
                    command_list.setTextureState(color_target, GfxAllSubresources, GfxResourceStates::ShaderResource);
                    command_list.commitBarriers();
                    return;
                }

                auto* shared_service = ctx.getSharedRenderService();
                const auto* shader_library = shared_service ? shared_service->getShaderLibrary() : nullptr;
                const auto* pipeline_cache = shared_service ? shared_service->getPipelineStateCache() : nullptr;
                if (!shader_library || !pipeline_cache) {
                    DO_ERROR("SpritePass: shader library or pipeline cache unavailable");
                    command_list.setTextureState(color_target, GfxAllSubresources, GfxResourceStates::ShaderResource);
                    command_list.commitBarriers();
                    return;
                }

                const auto instance_buffer = ctx.resolveBuffer(parameters.instance_buffer);
                const auto vp_buffer = ctx.resolveBuffer(parameters.vp_buffer);
                const auto quad_vertex_buffer = ctx.resolveBuffer(parameters.quad_vertex_buffer);
                const auto quad_index_buffer = ctx.resolveBuffer(parameters.quad_index_buffer);

                // Upload instance data and VP
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

                const Bool use_bindless = RenderSettings::IsBindlessActive();

                if (use_bindless) {
                    const auto* descriptor_manager = shared_service ? shared_service->getDescriptorTable() : nullptr;
                    if (!descriptor_manager || !descriptor_manager->getDescriptorTable() ||
                        !m_cb_binding_layout || !m_sampler_binding_layout) {
                        DO_ERROR("SpritePass: bindless sprite resources are unavailable");
                        command_list.setTextureState(color_target, GfxAllSubresources, GfxResourceStates::ShaderResource);
                        command_list.commitBarriers();
                        return;
                    }

                    const auto cb_binding_set = command_list.createBindingSet(
                        GfxBindingSetDesc()
                            .addItem(GfxBindingSetItem::ConstantBuffer(0, vp_buffer->getRHI())),
                        m_cb_binding_layout);
                    const auto sampler_binding_set = command_list.createBindingSet(
                        GfxBindingSetDesc()
                            .addItem(GfxBindingSetItem::Sampler(1, GlobalSamplers::screen().Get())),
                        m_sampler_binding_layout);
                    if (!cb_binding_set || !sampler_binding_set) {
                        DO_ERROR("SpritePass: failed to create bindless binding sets");
                        return;
                    }

                    const auto descriptor_binding_set = create_ref<GfxBindingSet>(
                        cutie::BindingSetHandle(descriptor_manager->getDescriptorTable()));
                    GfxGraphicsPipelineDesc pipeline_desc = GfxGraphicsPipelineDesc()
                        .setVertexShader(shader_library->getSpriteVertexShader())
                        .setPixelShader(shader_library->getSpritePixelShader())
                        .setInputLayout(m_input_layout)
                        .addBindingLayout(m_cb_binding_layout)
                        .addBindingLayout(m_sampler_binding_layout)
                        .addBindingLayout(descriptor_manager->getDescriptorTable()->getLayout())
                        .setPrimType(GfxPrimitiveType::TriangleList);
                    GfxDepthStencilState depth_stencil;
                    if (parameters.depth_occlusion) {
                        depth_stencil.enableDepthTest().setDepthFunc(GfxComparisonFunc::LessOrEqual).disableDepthWrite();
                    } else {
                        depth_stencil.disableDepthTest().disableDepthWrite();
                    }
                    depth_stencil.disableStencil();
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
                        DO_ERROR("SpritePass: failed to create bindless pipeline");
                        return;
                    }

                    DynamicArray<GfxBindingSetHandle> binding_sets = {cb_binding_set, sampler_binding_set, descriptor_binding_set};
                    DynamicArray<GfxVertexBufferBinding> vertex_buffers = {
                        GfxVertexBufferBinding().setBuffer(quad_vertex_buffer->getRHIHandle()).setSlot(0).setOffset(0),
                        GfxVertexBufferBinding().setBuffer(instance_buffer->getRHIHandle()).setSlot(1).setOffset(0)};
                    command_list.setGraphicsState(
                        ctx.getFramebuffer(), pipeline, binding_sets,
                        rendering_pipeline_utils::BuildViewportState(*ctx.getView(), ctx.getGfxContext()->getSwapchainExtent2D()),
                        vertex_buffers,
                        GfxIndexBufferBinding().setBuffer(quad_index_buffer->getRHIHandle()).setFormat(GfxFormat::R16_UINT));
                    command_list.drawIndexed(GfxDrawArguments()
                        .setVertexCount(6)
                        .setInstanceCount(static_cast<UInt32>(parameters.instances.size())));

                } else {
                    if (!m_cb_binding_layout || !m_material_binding_layout || !shared_service) {
                        DO_ERROR("SpritePass: array binding layout unavailable");
                        command_list.setTextureState(color_target, GfxAllSubresources, GfxResourceStates::ShaderResource);
                        command_list.commitBarriers();
                        return;
                    }

                    // 实例数据已在 build 阶段按 (sorting_key, atlas_index) 排序并写入缓冲区
                    const auto& sorted_instances = parameters.instances;

                    GfxDepthStencilState depth_stencil;
                    if (parameters.depth_occlusion) {
                        depth_stencil.enableDepthTest().setDepthFunc(GfxComparisonFunc::LessOrEqual).disableDepthWrite();
                    } else {
                        depth_stencil.disableDepthTest().disableDepthWrite();
                    }
                    depth_stencil.disableStencil();
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

                    const Size_t total = sorted_instances.size();
                    Size_t start = 0;
                    while (start < total) {
                        Size_t end = start + 1;
                        while (end < total && sorted_instances[end].atlas_index == sorted_instances[start].atlas_index) {
                            ++end;
                        }

                        const UInt32 slot = sorted_instances[start].atlas_index;
                        const auto tex_handle = shared_service->resolveTextureBySlot(slot);
                        if (!tex_handle) {
                            start = end;
                            continue;
                        }

                        auto cb_binding_set = command_list.createBindingSet(
                            GfxBindingSetDesc()
                                .addItem(GfxBindingSetItem::ConstantBuffer(0, vp_buffer->getRHI())),
                            m_cb_binding_layout);
                        auto material_binding_set = command_list.createBindingSet(
                            GfxBindingSetDesc()
                                .addItem(GfxBindingSetItem::Texture_SRV(2, tex_handle->getRHIHandle().Get()))
                                .addItem(GfxBindingSetItem::Sampler(1, GlobalSamplers::screen().Get())),
                            m_material_binding_layout);

                        if (!cb_binding_set || !material_binding_set) {
                            start = end;
                            continue;
                        }

                        GfxGraphicsPipelineDesc pipeline_desc;
                        pipeline_desc
                            .setVertexShader(shader_library->getSpriteVertexShader())
                            .setPixelShader(shader_library->getSpritePixelShaderTraditional())
                            .setInputLayout(m_input_layout)
                            .addBindingLayout(m_cb_binding_layout)
                            .addBindingLayout(m_material_binding_layout)
                            .setPrimType(GfxPrimitiveType::TriangleList)
                            .setRenderState(render_state);

                        const auto pipeline = pipeline_cache->resolveGraphicsPipeline(
                            pipeline_desc, ctx.getRenderTargetSignature(), command_list);
                        if (!pipeline) {
                            start = end;
                            continue;
                        }

                        DynamicArray<GfxBindingSetHandle> binding_sets = {cb_binding_set, material_binding_set};
                        DynamicArray<GfxVertexBufferBinding> vertex_buffers = {
                            GfxVertexBufferBinding().setBuffer(quad_vertex_buffer->getRHIHandle()).setSlot(0).setOffset(0),
                            GfxVertexBufferBinding().setBuffer(instance_buffer->getRHIHandle()).setSlot(1).setOffset(0)};

                        const auto viewport_state = rendering_pipeline_utils::BuildViewportState(
                            *ctx.getView(), ctx.getGfxContext()->getSwapchainExtent2D());
                        command_list.setGraphicsState(
                            ctx.getFramebuffer(), pipeline, binding_sets,
                            viewport_state, vertex_buffers,
                            GfxIndexBufferBinding().setBuffer(quad_index_buffer->getRHIHandle()).setFormat(GfxFormat::R16_UINT));
                        command_list.drawIndexed(GfxDrawArguments()
                            .setVertexCount(6)
                            .setInstanceCount(static_cast<UInt32>(end - start))
                            .setStartInstanceLocation(static_cast<UInt32>(start)));

                        start = end;
                    }
                }

                command_list.setTextureState(color_target, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.commitBarriers();
            });
    }

} // namespace dodoe
