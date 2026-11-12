// do@Redlive

#include "dopch.h"

#ifdef DODOE_EDITOR_ENABLED

#include "render_gizmo_pass.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"
#include "runtime/core/channel/gizmo_channel.h"

#include "../render_pipeline_pass_utils.h"

#include "runtime/function/render/pipeline_state/pipeline_state_cache.h"
#include "runtime/function/render/render_service/binding_layout_cache.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/texture/texture_manager.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/function/render/shader/global_samplers.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "render_pass_blackboard_keys.h"


namespace dodoe {

    struct EditorGridConstants {
        Matrix4f inverse_view_projection{1.0f};
        Matrix4f view_projection{1.0f};
        Vector4f camera_world{0.0f, 0.0f, 0.0f, 1.0f};
        Vector4f plane{0.0f, 1.0f, 0.0f, 0.0f};
        Vector4f params{1.0f, 10.0f, 40.0f, 300.0f};
    };

    struct GizmoPassParameters {
        RenderGraphTextureHandle color_target{};
        RenderGraphTextureHandle depth{};
        RenderGraphBufferHandle vertex_buffer{};
        RenderGraphBufferHandle index_buffer{};
        RenderGraphBufferHandle grid_buffer{};
        RenderGraphTextureHandle grid_depth{};
        GizmoChannelData gizmo_data{};
    };

    void GizmoPass::build(RenderGraphBuilder& graph,
                           const RenderPassBuildContext& context) {
        if (!context.view.hasViewFlag(RenderView::kShowEditorPrimitives)) return;

        if (context.shared_render_service) {
            auto* binding_layout_cache = context.shared_render_service->getBindingLayoutCache();
            if (binding_layout_cache) {
                m_grid_binding_layout = binding_layout_cache->getOrCreate(
                    GfxBindingLayoutDesc()
                        .setVisibility(GfxShaderType::Pixel)
                        .setRegisterSpaceIsDescriptorSet(true)
                        .setRegisterSpace(static_cast<UInt32>(ShaderParameterSet::Pass))
                        .addItem(GfxBindingLayoutItem::ConstantBuffer(0))
                        .addItem(GfxBindingLayoutItem::Texture_SRV(1))
                        .addItem(GfxBindingLayoutItem::Sampler(9)));
            }
        }

        graph.addPass<GizmoPassParameters>(
            "GizmoPass",
            RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
            [&context](RenderGraphPassBuilder& pass_builder, GizmoPassParameters& parameters) {
                const auto& channel_data = GetGizmoChannel().get<GizmoChannelData>();
                parameters.gizmo_data = channel_data;
                const auto* scene_color = pass_builder.blackboard().get<SceneColorKey>();
                RenderGraphAttachmentInfo color_attachment{};
                color_attachment.load_op = LoadOp::Load;
                if (scene_color) {
                    parameters.color_target = pass_builder.writeColor(*scene_color, color_attachment);
                } else {
                    color_attachment.load_op = LoadOp::Clear;
                    const auto swapchain_extent = context.gfx_context->getSwapchainExtent2D();
                    parameters.color_target = pass_builder.writeColor(pass_builder.createTransientTexture(
                        rendering_pipeline_utils::MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA8_UNORM, "RDG GizmoColor"),
                        "GizmoColor"), color_attachment);
                    pass_builder.blackboard().set<SceneColorKey>(parameters.color_target);
                }

                const auto* scene_textures = pass_builder.blackboard().get<SceneTexturesKey>();
                if (scene_textures && scene_textures->depth.isValid()) {
                    parameters.depth = pass_builder.read(scene_textures->depth);
                }

                RenderGraphBufferDesc vb_desc{};
                vb_desc.desc = GfxBufferDesc()
                    .setByteSize(std::max<Size_t>(parameters.gizmo_data.vertices.size(), 1) * sizeof(GizmoVertex))
                    .setIsVertexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
                    .setDebugName("RDG GizmoVB");
                parameters.vertex_buffer = pass_builder.createTransientBuffer(vb_desc, "GizmoVertexBuffer");
                if (!parameters.gizmo_data.vertices.empty()) {
                    pass_builder.uploadBuffer(parameters.vertex_buffer, parameters.gizmo_data.vertices.data(),
                        parameters.gizmo_data.vertices.size() * sizeof(GizmoVertex));
                    pass_builder.readBuffer(parameters.vertex_buffer, RenderGraphBufferUsage::Vertex);
                }

                RenderGraphBufferDesc ib_desc{};
                ib_desc.desc = GfxBufferDesc()
                    .setByteSize(std::max<Size_t>(parameters.gizmo_data.indices.size(), 1) * sizeof(UInt32))
                    .setIsIndexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::CopyDest)
                    .setDebugName("RDG GizmoIB");
                parameters.index_buffer = pass_builder.createTransientBuffer(ib_desc, "GizmoIndexBuffer");
                if (!parameters.gizmo_data.indices.empty()) {
                    pass_builder.uploadBuffer(parameters.index_buffer, parameters.gizmo_data.indices.data(),
                        parameters.gizmo_data.indices.size() * sizeof(UInt32));
                    pass_builder.readBuffer(parameters.index_buffer, RenderGraphBufferUsage::Index);
                }
                const auto* fallback = context.shared_render_service->getTextureManager()->getFallback();
                parameters.grid_depth = parameters.depth.isValid() ? parameters.depth
                    : pass_builder.read(pass_builder.importTexture(fallback->getGpuHandle(), "GridFallback"));
                EditorGridConstants grid_constants{};
                grid_constants.inverse_view_projection =
                    Math::Inverse(context.view.getViewProjectionMatrix());
                grid_constants.view_projection = context.view.getViewProjectionMatrix();
                grid_constants.camera_world = Vector4f(
                    rendering_pipeline_utils::ExtractCameraPosition(context.view),
                    parameters.depth.isValid() ? 1.0f : 0.0f);
                grid_constants.plane = parameters.gizmo_data.grid.ortho2d
                    ? Vector4f(0.0f, 0.0f, 1.0f, 0.0f)
                    : Vector4f(0.0f, 1.0f, 0.0f, 0.0f);
                grid_constants.params = Vector4f(
                    parameters.gizmo_data.grid.minor_spacing,
                    parameters.gizmo_data.grid.major_spacing,
                    parameters.gizmo_data.grid.fade_begin,
                    parameters.gizmo_data.grid.fade_end);

                RenderGraphBufferDesc grid_desc{};
                grid_desc.desc.setByteSize(sizeof(EditorGridConstants)).setIsConstantBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::CopyDest).setDebugName("EditorGridConstants");
                parameters.grid_buffer = pass_builder.createTransientBuffer(grid_desc, "EditorGridConstants");
                pass_builder.uploadBuffer(parameters.grid_buffer, &grid_constants, sizeof(grid_constants));
                pass_builder.readBuffer(parameters.grid_buffer, RenderGraphBufferUsage::Constant);
            },
            [this](const GizmoPassParameters& parameters, const RenderGraphPassContext& ctx, DrawCommandList& command_list) {
                const auto vb = ctx.resolveBuffer(parameters.vertex_buffer);
                const auto ib = ctx.resolveBuffer(parameters.index_buffer);

                struct GizmoPushConstants {
                    Matrix4f mvp;
                    Vector4f color;
                };

                const auto* shader_library = ctx.getShaderLibrary();
                const auto* pso_cache = ctx.getPipelineStateCache();
                const auto vs = shader_library->getGizmoVertexShader();
                const auto ps = shader_library->getGizmoPixelShader();

                const auto& gizmo_data = parameters.gizmo_data;

                if (m_grid_binding_layout) {
                    const auto grid_vs = shader_library->getFullscreenVertexShader();
                    const auto grid_ps = shader_library->getEditorGridPixelShader();
                    const auto depth_binding = ctx.resolveTexture(parameters.grid_depth);
                    if (grid_vs && grid_ps) {
                        const auto grid_binding_set = command_list.createBindingSet(
                            GfxLogicalBindingSetDesc()
                                .addItem(GfxLogicalBindingItem::ConstantBuffer(
                                    0, ctx.resolveBuffer(parameters.grid_buffer)))
                                .addItem(GfxLogicalBindingItem::Texture_SRV(1, depth_binding))
                                .addItem(GfxLogicalBindingItem::Sampler(9, GlobalSamplers::Screen())),
                            m_grid_binding_layout);
                        if (grid_binding_set) {
                            const auto grid_pipeline = pso_cache->resolveGraphicsPipeline(
                                rendering_pipeline_utils::BuildFullscreenPipelineDesc(
                                    grid_vs, grid_ps, m_grid_binding_layout, false, true),
                                ctx.getRenderTargetSignature(), command_list);
                            if (grid_pipeline) {
                                command_list.setGraphicsState(
                                    ctx.getFramebuffer(), grid_pipeline,
                                    DynamicArray<GfxBindingSetHandle>{grid_binding_set},
                                    rendering_pipeline_utils::BuildViewportState(
                                        *ctx.getView(), ctx.getGfxContext()->getSwapchainExtent2D()));
                                command_list.draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
                            }
                        }
                    }
                }

                if (gizmo_data.vertices.empty() || gizmo_data.commands.empty()) {
                    return;
                }

                GfxDepthStencilState ds;
                ds.disableDepthTest().disableDepthWrite().disableStencil();
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
                render_state.setDepthStencilState(ds).setRasterState(raster).setBlendState(blend);

                if (!m_push_constant_binding_set) {
                    m_push_constant_binding_set = command_list.createBindingSet(
                        GfxLogicalBindingSetDesc().addItem(GfxLogicalBindingItem::PushConstants(
                            0, static_cast<UInt32>(sizeof(GizmoPushConstants)))),
                        m_binding_layout);
                }
                if (!m_push_constant_binding_set) {
                    DO_ERROR("GizmoPass: failed to create push constant binding set");
                    return;
                }
                DynamicArray<GfxBindingSetHandle> binding_sets = {m_push_constant_binding_set};

                const auto view_projection = ctx.getView()->getViewProjectionMatrix();
                const auto framebuffer = ctx.getFramebuffer();

                GizmoPushConstants push{};
                push.color = Vector4f(1.0f, 1.0f, 1.0f, 1.0f);

                for (const auto& cmd : gizmo_data.commands) {
                    if (cmd.vertex_count == 0) {
                        continue;
                    }

                    auto pipeline_desc = GfxGraphicsPipelineDesc()
                        .setVertexShader(vs)
                        .setPixelShader(ps)
                        .setInputLayout(m_input_layout)
                        .addBindingLayout(m_binding_layout)
                        .setPrimType(cmd.topology)
                        .setRenderState(render_state);
                    auto pipeline = pso_cache->resolveGraphicsPipeline(
                        pipeline_desc, ctx.getRenderTargetSignature(), command_list);
                    if (!pipeline) {
                        DO_ERROR("GizmoPass: failed to create pipeline for topology={}",
                                 static_cast<UInt32>(cmd.topology));
                        continue;
                    }

                    push.mvp = view_projection * cmd.transform;

                    DynamicArray<GfxVertexBindingDesc> vbs;
                    vbs.push_back(GfxVertexBindingDesc()
                        .setBuffer(vb).setSlot(0).setOffset(cmd.vertex_offset * sizeof(GizmoVertex)));

                    GfxIndexBindingDesc index_binding;
                    if (cmd.index_count > 0) {
                        index_binding = GfxIndexBindingDesc()
                            .setBuffer(ib)
                            .setFormat(GfxFormat::R32_UINT)
                            .setOffset(cmd.index_offset * sizeof(UInt32));
                    }

                    const auto viewport_state = rendering_pipeline_utils::BuildViewportState(*ctx.getView(), ctx.getGfxContext()->getSwapchainExtent2D());
                    command_list.setGraphicsState(framebuffer, pipeline, binding_sets, viewport_state, vbs, index_binding);
                    command_list.setPushConstants(&push, sizeof(push));

                    if (cmd.index_count > 0) {
                        command_list.drawIndexed(GfxDrawArguments().setVertexCount(cmd.index_count));
                    } else {
                        command_list.draw(GfxDrawArguments().setVertexCount(cmd.vertex_count));
                    }
                }
            }
        );
    }

} // namespace dodoe

#endif
