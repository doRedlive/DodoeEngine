// do@Redlive

#include "runtime/function/render/render_pipeline/passes/render_gbuffer_pass.h"

#include <chrono>

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "runtime/function/render/render_pipeline/passes/render_pass_blackboard_keys.h"

#include "runtime/function/render/render_view/render_view.h"
#include "runtime/function/render/render_view/mesh_view_extension.h"
#include "runtime/function/render/render_pipeline/render_pipeline_pass_utils.h"
#include "runtime/function/render/render_settings.h"

#include "runtime/function/render/mesh_draw/lit_mesh_processor.h"
#include "runtime/function/render/mesh_draw/mesh_draw_list.h"
#include "runtime/function/render/mesh_draw/mesh_processor_base.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_service/render_target_handle.h"
#include "runtime/function/render/render_pipeline/render_feature/lit_scene_feature.h"
#include "runtime/function/render/gpu_driven/gpu_driven_renderer.h"
#include "runtime/function/render/render_scene/render_scene.h"
#include "runtime/function/render/render_pipeline/render_graph_import_keys.h"

namespace dodoe {

    struct GBufferPassParameters {
        RenderGraphTextureHandle albedo{};
        RenderGraphTextureHandle normal{};
        RenderGraphTextureHandle position{};
        RenderGraphTextureHandle material{};
        RenderGraphTextureHandle emissive{};
        RenderGraphTextureHandle depth{};
        RenderGraphBufferHandle primitive_scene_buffer{};
        RenderTargetHandle* gbuffer_rt{nullptr};
    };

    void GBufferPass::build(RenderGraphBuilder& graph,
                             const RenderPassBuildContext& context) {
        DO_ASSERT(m_mesh_processor != nullptr, "GBufferPass requires mesh processor");

        graph.addPass<GBufferPassParameters>(
            "GBufferPass",
            RenderGraphPassFlags::Raster,
            [view = &context.view, imports = context.graph_imports]
            (RenderGraphPassBuilder& b, GBufferPassParameters& p) {
                const auto* mesh_ext = view->getExtension<MeshViewExtension>();
                const Size_t visible_instance_count = mesh_ext ? mesh_ext->instance_scene_data.size() : 0;

                DO_ASSERT(imports != nullptr, "GBufferPass graph imports are null");
                p.gbuffer_rt = imports->require<GBufferRenderTargetKey>();
                DO_ASSERT(p.gbuffer_rt != nullptr, "GBufferPass requires a GBuffer RenderTargetHandle");

                RenderGraphAttachmentInfo color_attach{};
                color_attach.load_op = LoadOp::Clear;
                color_attach.clear_color = GfxColor(0.08f, 0.09f, 0.11f, 1.0f);
                p.albedo = b.writeColor(b.importTexture(p.gbuffer_rt->getColorTexture(0), "GBufferAlbedo"), color_attach);

                color_attach.clear_color = GfxColor(0.0f, 0.0f, 0.0f, 1.0f);
                p.normal = b.writeColor(b.importTexture(p.gbuffer_rt->getColorTexture(1), "GBufferNormal"), color_attach);

                color_attach.clear_color = GfxColor(0.0f, 0.0f, 0.0f, 1.0f);
                p.position = b.writeColor(b.importTexture(p.gbuffer_rt->getColorTexture(2), "GBufferPosition"), color_attach);

                color_attach.clear_color = GfxColor(0.0f, 1.0f, 1.0f, 1.0f);
                p.material = b.writeColor(b.importTexture(p.gbuffer_rt->getColorTexture(3), "GBufferMaterial"), color_attach);

                color_attach.clear_color = GfxColor(0.0f, 0.0f, 0.0f, 1.0f);
                p.emissive = b.writeColor(b.importTexture(p.gbuffer_rt->getColorTexture(4), "GBufferEmissive"), color_attach);

                RenderGraphAttachmentInfo depth_attach{};
                depth_attach.load_op = LoadOp::Clear;
                p.depth = b.writeDepth(b.importTexture(p.gbuffer_rt->getDepthTexture(), "GBufferDepth"), depth_attach);

                RenderGraphBufferDesc primitive_scene_buffer_desc{};
                primitive_scene_buffer_desc.desc = GfxBufferDesc()
                    .setByteSize(static_cast<UInt32>(std::max<Size_t>(visible_instance_count, 1) * sizeof(InstanceSceneData)))
                    .setIsVertexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::VertexBuffer)
                    .setDebugName("RDG GBufferPass PrimitiveSceneBuffer");
                p.primitive_scene_buffer = b.write(b.createTransientBuffer(primitive_scene_buffer_desc, "GBufferPrimitiveSceneBuffer"));

                SceneTextures gbuffer;
                gbuffer.albedo   = p.albedo;
                gbuffer.normal   = p.normal;
                gbuffer.position = p.position;
                gbuffer.material = p.material;
                gbuffer.emissive = p.emissive;
                gbuffer.depth    = p.depth;
                gbuffer.instance_scene_data = p.primitive_scene_buffer;
                b.blackboard().set<SceneTexturesKey>(gbuffer);
            },
            [this, processor = m_mesh_processor](const GBufferPassParameters& p, const RenderGraphPassContext& ctx, DrawCommandList& command_list) {
                DO_ASSERT(ctx.getView() != nullptr, "GBufferPass view is null");

                const auto viewport_state = rendering_pipeline_utils::BuildViewportState(*ctx.getView(), ctx.getGfxContext()->getSwapchainExtent2D());

                const auto* mesh_ext = ctx.getView()->getExtension<MeshViewExtension>();
                const auto& instance_data = mesh_ext->instance_scene_data;
                const auto resolved_psb = ctx.resolveBuffer(p.primitive_scene_buffer);
                command_list.setBufferState(resolved_psb, GfxResourceStates::CopyDest);
                command_list.commitBarriers();
                command_list.writeBuffer(resolved_psb, instance_data.data(), instance_data.size() * sizeof(InstanceSceneData));
                command_list.setBufferState(resolved_psb, GfxResourceStates::VertexBuffer);

                const GlobalMeshShaderData global_data{mesh_ext->frame_time_data};
                command_list.writeBuffer(processor->getGlobalConstantBuffer(), &global_data, sizeof(global_data));
                const ViewMeshShaderData view_data{ctx.getView()->getViewProjectionMatrix()};
                command_list.writeBuffer(processor->getViewConstantBuffer(), &view_data, sizeof(view_data));

                auto* feature = static_cast<LitSceneFeature*>(m_owning_feature);
                DO_ASSERT(feature != nullptr, "GBufferPass owning feature is null");
                const auto fb = ctx.getFramebuffer();

                auto* gpu_culling = feature->getGpuCulling();
                auto* gpu_scene = ctx.getScene() ? ctx.getScene()->getGpuScene() : nullptr;
                const auto& gpu_scene_resources = gpu_scene ? gpu_scene->getPassResources() : GpuScenePassResources{};
                const auto& gpu_buckets = feature->getGpuBuckets(ctx.getViewIndex());
                DynamicArray<GpuBucketCpuDraw> gpu_draws;
                const Bool gpu_active = RenderSettings::GetResolvedFeatures().gpu_driven_active;
                Bool use_gpu_draws = false;
                if (gpu_active) {
                    const Bool has_culling = gpu_culling && gpu_culling->isEnabled();
                    const Bool has_instance_buffer = gpu_scene_resources.primitive_render_instance
                        && gpu_scene_resources.primitive_render_instance->getRHI();
                    const Bool has_buckets = !gpu_buckets.empty();
                    const Bool has_draws = has_culling && has_instance_buffer && has_buckets
                        && gpu_culling->acquireBucketDraws(gpu_draws);
                    use_gpu_draws = has_draws;
                    if (!use_gpu_draws) {
                        static auto last_fallback_warn = std::chrono::steady_clock::now() - std::chrono::seconds(2);
                        const auto now = std::chrono::steady_clock::now();
                        if (now - last_fallback_warn >= std::chrono::seconds(1)) {
                            last_fallback_warn = now;
                            DO_WARN("GBufferPass: gpu-driven fallback (culling={}, instance_buffer={}, buckets={}, draws={})",
                                has_culling, has_instance_buffer, gpu_buckets.size(), gpu_draws.size());
                        }
                    }
                }

                if (use_gpu_draws) {
                    const auto indirect_args = gpu_culling->getIndirectArgsBuffer(1);
                    const auto instance_binding = GfxVertexBufferBinding()
                        .setBuffer(gpu_scene_resources.primitive_render_instance->getRHI())
                        .setSlot(1)
                        .setOffset(0);
                    const auto& draw_snapshots = gpu_culling->getBucketDrawSnapshots(1);
                    for (const auto& draw : gpu_draws) {
                        if (draw.template_index >= draw_snapshots.size()) {
                            continue;
                        }
                        const auto& snapshot = draw_snapshots[draw.template_index];
                        if (!snapshot.pipeline) {
                            continue;
                        }
                        command_list.writeBuffer(processor->getPrimitiveConstantBuffer(), snapshot.shader_data);
                        DynamicArray<GfxVertexBufferBinding> vertex_bindings = snapshot.vertex_bindings;
                        vertex_bindings.push_back(instance_binding);
                        command_list.setGraphicsState(fb, snapshot.pipeline, snapshot.binding_sets,
                            viewport_state, vertex_bindings, snapshot.index_binding, indirect_args);
                        command_list.setBufferState(indirect_args, GfxResourceStates::IndirectArgument);
                        command_list.commitBarriers();
                        command_list.drawIndexedIndirect(
                            draw.first_arg * sizeof(DrawIndexedIndirectArgs), draw.arg_count);
                    }
                } else {
                    const auto& draw_list = feature->getLitDrawLists()[ctx.getViewIndex()];
                    SubmitMeshDrawSources(draw_list.sources, processor->getPrimitiveConstantBuffer(),
                        fb, viewport_state, resolved_psb, nullptr, command_list);
                }
            }
        );
    }

} // namespace dodoe
