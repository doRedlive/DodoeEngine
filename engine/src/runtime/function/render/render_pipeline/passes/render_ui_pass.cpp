// do@Redlive

#include "render_ui_pass.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"
#include "../render_pipeline_pass_utils.h"
#include "render_pass_blackboard_keys.h"

#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/render_scene/ui_scene_info.h"
#include "runtime/function/render/gpu_driven/gpu_scene.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/shader/descriptor_table_manager.h"
#include "runtime/function/render/shader/global_samplers.h"
#include "runtime/function/render/render_settings.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/pipeline_state/pipeline_state_cache.h"
#include "runtime/core/math/math.h"

#include <algorithm>

namespace dodoe {

    struct UIPassParameters {
        RenderGraphTextureHandle color_target{};
        RenderGraphBufferHandle instance_buffer{};
        RenderGraphBufferHandle quad_vertex_buffer{};
        RenderGraphBufferHandle quad_index_buffer{};
        RenderGraphBufferHandle vp_buffer{};
        FrameArray<UIInstance> instances{};
    };

    void UIPass::build(RenderGraphBuilder& graph,
                       const RenderPassBuildContext& context) {
        graph.addPass<UIPassParameters>(
            "UIPass",
            RenderGraphPassFlags::Raster,
            [&context](RenderGraphPassBuilder& pass_builder, UIPassParameters& parameters) {
                const auto swapchain_extent = context.gfx_context->getSwapchainExtent2D();
                const auto* scene_color = pass_builder.blackboard().get<SceneColorKey>();
                RenderGraphAttachmentInfo color_attachment{};
                color_attachment.load_op = scene_color ? LoadOp::Load : LoadOp::Clear;
                color_attachment.clear_color = GfxColor(0.0f, 0.0f, 0.0f, 1.0f);
                if (scene_color) {
                    parameters.color_target = pass_builder.writeColor(*scene_color, color_attachment);
                } else {
                    parameters.color_target = pass_builder.writeColor(pass_builder.createTransientTexture(
                        rendering_pipeline_utils::MakeSwapchainRT2D(
                            swapchain_extent, GfxFormat::RGBA8_UNORM, "RDG UIColor"),
                        "UIColor"), color_attachment);
                    pass_builder.blackboard().set<SceneColorKey>(parameters.color_target);
                }

                if (context.scene) {
                    const auto& ui_infos = context.scene->getUISceneInfo();
                    parameters.instances.reserve(ui_infos.size());
                    for (const auto& info : ui_infos) {
                        parameters.instances.push_back(info.toInstance());
                    }

                }

                // Instance buffer
                RenderGraphBufferDesc instance_desc{};
                instance_desc.desc = GfxBufferDesc()
                    .setByteSize(static_cast<UInt32>(std::max<Size_t>(parameters.instances.size(), 1) * sizeof(UIInstance)))
                    .setIsVertexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
                    .setDebugName("RDG VisibleUIInstances");
                parameters.instance_buffer = pass_builder.createTransientBuffer(instance_desc, "VisibleUIInstances");
                pass_builder.uploadBuffer(parameters.instance_buffer, parameters.instances.data(),
                    parameters.instances.size() * sizeof(UIInstance));
                if (!parameters.instances.empty())
                    pass_builder.readBuffer(parameters.instance_buffer, RenderGraphBufferUsage::Vertex);

                // VP constant buffer (orthographic projection)
                RenderGraphBufferDesc vp_desc{};
                vp_desc.desc = GfxBufferDesc()
                    .setByteSize(sizeof(Matrix4f))
                    .setIsConstantBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
                    .setDebugName("RDG UIVP");
                parameters.vp_buffer = pass_builder.createTransientBuffer(vp_desc, "UIVpBuffer");
                const auto rect = context.view.getViewportRect();
                Matrix4f projection = Math::OrthoRH_ZO(
                    0.0f, static_cast<Float>(rect.z > 0 ? rect.z : swapchain_extent.x),
                    static_cast<Float>(rect.w > 0 ? rect.w : swapchain_extent.y), 0.0f, 0.0f, 1.0f);
                projection = Math::FlipClipSpaceY(projection);
                pass_builder.uploadBuffer(parameters.vp_buffer, &projection, sizeof(projection));
                pass_builder.readBuffer(parameters.vp_buffer, RenderGraphBufferUsage::Constant);

                UnorderedSet<UInt32> texture_slots;
                for (const auto& instance : parameters.instances) {
                    if (!texture_slots.insert(instance.atlas_index).second) continue;
                    const auto texture = context.shared_render_service->resolveTextureBySlot(instance.atlas_index);
                    if (texture) pass_builder.read(pass_builder.importTexture(texture, "UIAtlas"));
                }


                // Quad mesh (reused from GpuScene)
                const auto* gpu_scene = context.scene ? context.scene->getGpuScene() : nullptr;
                if (!gpu_scene) {
                    return;
                }
                const auto scene_resources = gpu_scene->getPassResources();
                if (scene_resources.quad_vb && scene_resources.quad_ib) {
                    parameters.quad_vertex_buffer = pass_builder.readBuffer(pass_builder.importBuffer(scene_resources.quad_vb, "UIQuadVB"), RenderGraphBufferUsage::Vertex);
                    parameters.quad_index_buffer = pass_builder.readBuffer(pass_builder.importBuffer(scene_resources.quad_ib, "UIQuadIB"), RenderGraphBufferUsage::Index);
                }
            },
            [this](const UIPassParameters& parameters, const RenderGraphPassContext& ctx,
                   DrawCommandList& command_list) {
                if (parameters.instances.empty() || !parameters.quad_vertex_buffer.isValid() ||
                    !parameters.quad_index_buffer.isValid() || !m_input_layout) {
                    return;
                }

                auto* shared_service = ctx.getSharedRenderService();
                const auto* shader_library = shared_service ? shared_service->getShaderLibrary() : nullptr;
                const auto* pipeline_cache = shared_service ? shared_service->getPipelineStateCache() : nullptr;
                if (!shader_library || !pipeline_cache) {
                    DO_ERROR("UIPass: shader library or pipeline cache unavailable");
                    return;
                }

                const auto instance_buffer = ctx.resolveBuffer(parameters.instance_buffer);
                const auto vp_buffer = ctx.resolveBuffer(parameters.vp_buffer);
                const auto quad_vertex_buffer = ctx.resolveBuffer(parameters.quad_vertex_buffer);
                const auto quad_index_buffer = ctx.resolveBuffer(parameters.quad_index_buffer);

                const Bool use_bindless = RenderSettings::IsBindlessActive();

                GfxGraphicsPipelineDesc pipeline_desc;

                if (use_bindless) {
                    const auto* descriptor_manager = shared_service ? shared_service->getDescriptorTable() : nullptr;
                    if (!descriptor_manager || !descriptor_manager->getDescriptorTable() || !m_bindless_binding_layout) {
                        DO_ERROR("UIPass: bindless UI resources are unavailable");
                        return;
                    }

                    auto* set_cache = shared_service->getBindingSetCache();
                    const GfxLogicalBindingSetDesc view_desc = GfxLogicalBindingSetDesc()
                        .addItem(GfxLogicalBindingItem::ConstantBuffer(0, vp_buffer));
                    auto view_binding_set = command_list.createBindingSet(view_desc, m_view_binding_layout);
                    const GfxLogicalBindingSetDesc material_desc = GfxLogicalBindingSetDesc()
                        .addItem(GfxLogicalBindingItem::Sampler(1, GlobalSamplers::Screen()));
                    auto material_binding_set = set_cache->getOrCreate(command_list, material_desc, m_bindless_binding_layout, 0);
                    if (!view_binding_set || !material_binding_set) {
                        DO_ERROR("UIPass: failed to create bindless binding set");
                        return;
                    }

                    const auto descriptor_binding_set = create_ref<GfxBindingSet>(
                        cutie::BindingSetHandle(descriptor_manager->getDescriptorTable()));
                    DynamicArray<GfxBindingSetHandle> binding_sets = {view_binding_set, material_binding_set, descriptor_binding_set};

                    pipeline_desc = GfxGraphicsPipelineDesc()
                        .setVertexShader(shader_library->getUIVertexShader())
                        .setPixelShader(shader_library->getUIPixelShaderBindless())
                        .setInputLayout(m_input_layout)
                        .addBindingLayout(m_view_binding_layout)
                        .addBindingLayout(m_bindless_binding_layout)
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
                        return;
                    }

                    DynamicArray<GfxVertexBindingDesc> vertex_buffers = {
                        GfxVertexBindingDesc().setBuffer(quad_vertex_buffer).setSlot(0).setOffset(0),
                        GfxVertexBindingDesc().setBuffer(instance_buffer).setSlot(1).setOffset(0)};
                    const auto viewport_state = rendering_pipeline_utils::BuildViewportState(
                        *ctx.getView(), ctx.getGfxContext()->getSwapchainExtent2D());
                    command_list.setGraphicsState(
                        ctx.getFramebuffer(), pipeline, binding_sets,
                        viewport_state, vertex_buffers,
                        GfxIndexBindingDesc().setBuffer(quad_index_buffer).setFormat(GfxFormat::R16_UINT));
                    command_list.drawIndexed(GfxDrawArguments()
                        .setVertexCount(6)
                        .setInstanceCount(static_cast<UInt32>(parameters.instances.size())));
                    return;
                } else {
                    if (!m_view_binding_layout || !m_material_binding_layout || !shared_service) {
                        DO_ERROR("UIPass: array binding layout unavailable");
                        return;
                    }

                    const auto& sorted_instances = parameters.instances;

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

                        auto* set_cache = shared_service->getBindingSetCache();
                        const GfxLogicalBindingSetDesc view_desc = GfxLogicalBindingSetDesc()
                            .addItem(GfxLogicalBindingItem::ConstantBuffer(0, vp_buffer));
                        auto view_binding_set = command_list.createBindingSet(view_desc, m_view_binding_layout);
                        const GfxLogicalBindingSetDesc material_desc = GfxLogicalBindingSetDesc()
                            .addItem(GfxLogicalBindingItem::Texture_SRV(2, tex_handle))
                            .addItem(GfxLogicalBindingItem::Sampler(1, GlobalSamplers::Screen()));
                        auto material_binding_set = set_cache->getOrCreate(command_list, material_desc, m_material_binding_layout, 0);

                        if (!view_binding_set || !material_binding_set) {
                            start = end;
                            continue;
                        }

                        GfxGraphicsPipelineDesc pipeline_desc;
                        pipeline_desc
                            .setVertexShader(shader_library->getUIVertexShader())
                            .setPixelShader(shader_library->getUIPixelShaderArray())
                            .setInputLayout(m_input_layout)
                            .addBindingLayout(m_view_binding_layout)
                            .addBindingLayout(m_material_binding_layout)
                            .setPrimType(GfxPrimitiveType::TriangleList)
                            .setRenderState(render_state);

                        const auto pipeline = pipeline_cache->resolveGraphicsPipeline(
                            pipeline_desc, ctx.getRenderTargetSignature(), command_list);
                        if (!pipeline) {
                            start = end;
                            continue;
                        }

                        DynamicArray<GfxBindingSetHandle> binding_sets = {view_binding_set, material_binding_set};
                        DynamicArray<GfxVertexBindingDesc> vertex_buffers = {
                            GfxVertexBindingDesc().setBuffer(quad_vertex_buffer).setSlot(0).setOffset(0),
                            GfxVertexBindingDesc().setBuffer(instance_buffer).setSlot(1).setOffset(0)};

                        const auto viewport_state = rendering_pipeline_utils::BuildViewportState(
                            *ctx.getView(), ctx.getGfxContext()->getSwapchainExtent2D());
                        command_list.setGraphicsState(
                            ctx.getFramebuffer(), pipeline, binding_sets,
                            viewport_state, vertex_buffers,
                            GfxIndexBindingDesc().setBuffer(quad_index_buffer).setFormat(GfxFormat::R16_UINT));
                        command_list.drawIndexed(GfxDrawArguments()
                            .setVertexCount(6)
                            .setInstanceCount(static_cast<UInt32>(end - start))
                            .setStartInstanceLocation(static_cast<UInt32>(start)));

                        start = end;
                    }

                    return;
                }
            });
    }

} // namespace dodoe
