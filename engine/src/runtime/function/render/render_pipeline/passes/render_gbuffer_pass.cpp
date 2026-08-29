// do@Redlive

#include "runtime/function/render/render_pipeline/passes/render_gbuffer_pass.h"

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
#include "runtime/function/render/render_pipeline/render_graph_import_keys.h"

namespace dodoe {

    struct GBufferPassParameters {
        RenderGraphTextureHandle albedo{};
        RenderGraphTextureHandle normal{};
        RenderGraphTextureHandle position{};
        RenderGraphTextureHandle material{};
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
                const auto& draw_list = feature->getLitDrawLists()[ctx.getViewIndex()];

                const auto fb = ctx.getFramebuffer();
                SubmitMeshDrawCommands(draw_list.cached_instances, *draw_list.cached_commands,
                    draw_list.cached_shader_data, processor->getPrimitiveConstantBuffer(),
                    fb, viewport_state, resolved_psb, command_list);
                SubmitMeshDrawCommands(draw_list.dynamic_instances, draw_list.frame_commands,
                    draw_list.dynamic_shader_data, processor->getPrimitiveConstantBuffer(),
                    fb, viewport_state, resolved_psb, command_list);
            }
        );
    }

} // namespace dodoe
