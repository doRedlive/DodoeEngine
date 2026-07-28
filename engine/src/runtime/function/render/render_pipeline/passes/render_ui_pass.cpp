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
#include "runtime/function/render/pipeline/pipeline_state_cache.h"
#include "runtime/core/math/math.h"

#include <algorithm>

namespace dodoe {

    struct UIPassParameters {
        RenderGraphTextureHandle color_target{};
        Bool clear_target{false};
        RenderGraphBufferHandle instance_buffer{};
        RenderGraphBufferHandle quad_vertex_buffer{};
        RenderGraphBufferHandle quad_index_buffer{};
        RenderGraphBufferHandle vp_buffer{};
        DynamicArray<UIInstance> instances{};
    };

    void UIPass::build(RenderGraphBuilder& graph,
                       const RenderPassBuildContext& context) {
        graph.addPass<UIPassParameters>(
            "UIPass",
            RenderGraphPassFlags::Raster,
            [&context](RenderGraphPassBuilder& pass_builder, UIPassParameters& parameters) {
                const auto swapchain_extent = context.gfx_context->getSwapchainExtent2d();
                const auto* scene_color = pass_builder.blackboard().get<SceneColorKey>();
                if (scene_color) {
                    parameters.color_target = pass_builder.writeColor(*scene_color);
                } else {
                    parameters.clear_target = true;
                    parameters.color_target = pass_builder.writeColor(pass_builder.createTransientTexture(
                        rendering_pipeline_utils::MakeSwapchainRT2D(
                            swapchain_extent, GfxFormat::RGBA8_UNORM, "RDG UIColor"),
                        "UIColor"));
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
                parameters.instance_buffer = pass_builder.writeBuffer(
                    pass_builder.createTransientBuffer(instance_desc, "VisibleUIInstances"),
                    RenderGraphPipelineStage::Copy);
                pass_builder.readBuffer(parameters.instance_buffer, RenderGraphPipelineStage::VertexShader);

                // VP constant buffer (orthographic projection)
                RenderGraphBufferDesc vp_desc{};
                vp_desc.desc = GfxBufferDesc()
                    .setByteSize(sizeof(Matrix4f))
                    .setIsConstantBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
                    .setDebugName("RDG UIVP");
                parameters.vp_buffer = pass_builder.writeBuffer(
                    pass_builder.createTransientBuffer(vp_desc, "UIVpBuffer"),
                    RenderGraphPipelineStage::Copy);
                pass_builder.readBuffer(parameters.vp_buffer, RenderGraphPipelineStage::VertexShader);

                // Quad mesh (reused from GpuScene)
                const auto* gpu_scene = context.scene ? context.scene->getGpuScene() : nullptr;
                if (!gpu_scene) {
                    return;
                }
                const auto scene_resources = gpu_scene->getPassResources();
                if (scene_resources.quad_vb && scene_resources.quad_ib) {
                    parameters.quad_vertex_buffer = pass_builder.importBuffer(scene_resources.quad_vb, "UIQuadVB");
                    parameters.quad_index_buffer = pass_builder.importBuffer(scene_resources.quad_ib, "UIQuadIB");
                }
            },
            [this](const UIPassParameters& parameters, const RenderGraphPassContext& ctx,
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
                    DO_ERROR("UIPass: shader library or pipeline cache unavailable");
                    command_list.setTextureState(color_target, GfxAllSubresources, GfxResourceStates::ShaderResource);
                    command_list.commitBarriers();
                    return;
                }

                const auto instance_buffer = ctx.resolveBuffer(parameters.instance_buffer);
                const auto vp_buffer = ctx.resolveBuffer(parameters.vp_buffer);
                const auto quad_vertex_buffer = ctx.resolveBuffer(parameters.quad_vertex_buffer);
                const auto quad_index_buffer = ctx.resolveBuffer(parameters.quad_index_buffer);

                // Upload instance data
                command_list.setBufferState(instance_buffer, GfxResourceStates::CopyDest);
                command_list.setBufferState(vp_buffer, GfxResourceStates::CopyDest);
                command_list.commitBarriers();
                command_list.writeBuffer(instance_buffer, parameters.instances.data(),
                                         parameters.instances.size() * sizeof(UIInstance));

                // Build orthographic projection for screen-space rendering
                const auto extent = ctx.getGfxContext()->getSwapchainExtent2d();
                const Matrix4f ui_view_projection = Math::OrthoRH_ZO(
                    0.0f, static_cast<Float>(extent.x),
                    static_cast<Float>(extent.y), 0.0f,   // top-down Y
                    0.0f, 1.0f);
                command_list.writeBuffer(vp_buffer, &ui_view_projection, sizeof(ui_view_projection));

                command_list.setBufferState(instance_buffer, GfxResourceStates::VertexBuffer);
                command_list.setBufferState(vp_buffer, GfxResourceStates::ConstantBuffer);
                command_list.setBufferState(quad_vertex_buffer, GfxResourceStates::VertexBuffer);
                command_list.setBufferState(quad_index_buffer, GfxResourceStates::IndexBuffer);
                command_list.commitBarriers();

                const Bool use_bindless = RenderSettings::IsBindlessActive();

                GfxGraphicsPipelineDesc pipeline_desc;

                if (use_bindless) {
                    const auto* descriptor_manager = shared_service ? shared_service->getDescriptorTable() : nullptr;
                    if (!descriptor_manager || !descriptor_manager->getDescriptorTable() || !m_bindless_binding_layout) {
                        DO_ERROR("UIPass: bindless UI resources are unavailable");
                        command_list.setTextureState(color_target, GfxAllSubresources, GfxResourceStates::ShaderResource);
                        command_list.commitBarriers();
                        return;
                    }

                    auto binding_set = command_list.createBindingSet(
                        GfxBindingSetDesc()
                            .addItem(GfxBindingSetItem::ConstantBuffer(0, vp_buffer->getRHI()))
                            .addItem(GfxBindingSetItem::Sampler(0, GlobalSamplers::screen().Get())),
                        m_bindless_binding_layout);
                    if (!binding_set) {
                        DO_ERROR("UIPass: failed to create bindless binding set");
                        return;
                    }

                    const auto descriptor_binding_set = create_ref<GfxBindingSet>(
                        cutie::BindingSetHandle(descriptor_manager->getDescriptorTable()));
                    DynamicArray<GfxBindingSetHandle> binding_sets = {binding_set, descriptor_binding_set};

                    pipeline_desc = GfxGraphicsPipelineDesc()
                        .setVertexShader(shader_library->getUIVertexShader())
                        .setPixelShader(shader_library->getUIPixelShaderBindless())
                        .setInputLayout(m_input_layout)
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

                    auto framebuffer_desc = GfxFramebufferDesc().addColorAttachment(color_target);
                    auto framebuffer = command_list.createFramebuffer(framebuffer_desc);
                    GfxFramebufferInfo framebuffer_info(framebuffer_desc);
                    const auto pipeline = pipeline_cache->resolveGraphicsPipeline(
                        pipeline_desc, framebuffer_info, command_list);
                    if (!pipeline) {
                        command_list.setTextureState(color_target, GfxAllSubresources, GfxResourceStates::ShaderResource);
                        command_list.commitBarriers();
                        return;
                    }

                    DynamicArray<GfxVertexBufferBinding> vertex_buffers = {
                        GfxVertexBufferBinding().setBuffer(quad_vertex_buffer->getRHIHandle()).setSlot(0).setOffset(0),
                        GfxVertexBufferBinding().setBuffer(instance_buffer->getRHIHandle()).setSlot(1).setOffset(0)};
                    const auto viewport_state = rendering_pipeline_utils::BuildViewportState(
                        *ctx.getView(), ctx.getGfxContext()->getSwapchainExtent2d());
                    command_list.setGraphicsState(
                        framebuffer, pipeline, binding_sets,
                        viewport_state, vertex_buffers,
                        GfxIndexBufferBinding().setBuffer(quad_index_buffer->getRHIHandle()));
                    command_list.drawIndexed(GfxDrawArguments()
                        .setVertexCount(6)
                        .setInstanceCount(static_cast<UInt32>(parameters.instances.size())));
                    command_list.setTextureState(color_target, GfxAllSubresources, GfxResourceStates::ShaderResource);
                    command_list.commitBarriers();
                    return;
                } else {
                    if (!m_array_binding_layout || !shared_service) {
                        DO_ERROR("UIPass: array binding layout unavailable");
                        command_list.setTextureState(color_target, GfxAllSubresources, GfxResourceStates::ShaderResource);
                        command_list.commitBarriers();
                        return;
                    }

                    auto sorted_instances = parameters.instances;
                    std::sort(sorted_instances.begin(), sorted_instances.end(),
                        [](const UIInstance& a, const UIInstance& b) {
                            return a.atlas_index < b.atlas_index;
                        });

                    command_list.setBufferState(instance_buffer, GfxResourceStates::CopyDest);
                    command_list.commitBarriers();
                    command_list.writeBuffer(instance_buffer, sorted_instances.data(),
                                             sorted_instances.size() * sizeof(UIInstance));
                    command_list.setBufferState(instance_buffer, GfxResourceStates::VertexBuffer);
                    command_list.commitBarriers();

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

                    auto framebuffer_desc = GfxFramebufferDesc().addColorAttachment(color_target);
                    auto framebuffer = command_list.createFramebuffer(framebuffer_desc);
                    GfxFramebufferInfo framebuffer_info(framebuffer_desc);

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

                        auto binding_set = command_list.createBindingSet(
                            GfxBindingSetDesc()
                                .addItem(GfxBindingSetItem::ConstantBuffer(0, vp_buffer->getRHI()))
                                .addItem(GfxBindingSetItem::Sampler(0, GlobalSamplers::screen().Get()))
                                .addItem(GfxBindingSetItem::Texture_SRV(0, tex_handle->getRHIHandle().Get())),
                            m_array_binding_layout);

                        if (!binding_set) {
                            start = end;
                            continue;
                        }

                        GfxGraphicsPipelineDesc pipeline_desc;
                        pipeline_desc
                            .setVertexShader(shader_library->getUIVertexShader())
                            .setPixelShader(shader_library->getUIPixelShaderArray())
                            .setInputLayout(m_input_layout)
                            .addBindingLayout(m_array_binding_layout)
                            .setPrimType(GfxPrimitiveType::TriangleList)
                            .setRenderState(render_state);

                        const auto pipeline = pipeline_cache->resolveGraphicsPipeline(
                            pipeline_desc, framebuffer_info, command_list);
                        if (!pipeline) {
                            start = end;
                            continue;
                        }

                        DynamicArray<GfxBindingSetHandle> binding_sets = {binding_set};
                        DynamicArray<GfxVertexBufferBinding> vertex_buffers = {
                            GfxVertexBufferBinding().setBuffer(quad_vertex_buffer->getRHIHandle()).setSlot(0).setOffset(0),
                            GfxVertexBufferBinding().setBuffer(instance_buffer->getRHIHandle()).setSlot(1).setOffset(0)};

                        const auto viewport_state = rendering_pipeline_utils::BuildViewportState(
                            *ctx.getView(), ctx.getGfxContext()->getSwapchainExtent2d());
                        command_list.setGraphicsState(
                            framebuffer, pipeline, binding_sets,
                            viewport_state, vertex_buffers,
                            GfxIndexBufferBinding().setBuffer(quad_index_buffer->getRHIHandle()));
                        command_list.drawIndexed(GfxDrawArguments()
                            .setVertexCount(6)
                            .setInstanceCount(static_cast<UInt32>(end - start))
                            .setStartInstanceLocation(static_cast<UInt32>(start)));

                        start = end;
                    }

                    command_list.setTextureState(color_target, GfxAllSubresources, GfxResourceStates::ShaderResource);
                    command_list.commitBarriers();
                    return;
                }
            });
    }

} // namespace dodoe
