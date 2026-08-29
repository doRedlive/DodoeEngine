// do@Redlive

#include "runtime/function/render/render_pipeline/passes/render_shadow_pass.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "runtime/function/render/render_pipeline/passes/render_pass_blackboard_keys.h"

#include "runtime/function/render/render_view/render_view.h"
#include "runtime/function/render/render_view/mesh_view_extension.h"
#include "runtime/function/render/render_pipeline/render_pipeline_pass_utils.h"

#include "runtime/function/render/mesh_draw/shadow_mesh_processor.h"
#include "runtime/function/render/mesh_draw/mesh_draw_list.h"
#include "runtime/function/render/mesh_draw/mesh_processor_base.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_service/render_target_handle.h"
#include "runtime/function/render/render_pipeline/render_feature/shadow_scene_feature.h"
#include "runtime/function/render/render_pipeline/render_graph_import_keys.h"

namespace dodoe {

    struct ShadowPassParameters {
        RenderGraphTextureHandle shadow_map{};
        RenderGraphBufferHandle primitive_scene_buffer{};
        RenderTargetHandle* shadow_rt{nullptr};
    };

    void ShadowPass::build(RenderGraphBuilder& graph,
                           const RenderPassBuildContext& context) {
        DO_ASSERT(m_mesh_processor != nullptr, "ShadowPass requires mesh processor");

        graph.addPass<ShadowPassParameters>(
            "ShadowPass",
            RenderGraphPassFlags::Raster,
            [view = &context.view, imports = context.graph_imports]
            (RenderGraphPassBuilder& pass_builder, ShadowPassParameters& parameters) {
                const auto* scene_textures = pass_builder.blackboard().get<SceneTexturesKey>();
                DO_ASSERT(scene_textures, "ShadowPass scene textures are missing");

                DO_ASSERT(imports != nullptr, "ShadowPass graph imports are null");
                parameters.shadow_rt = imports->require<ShadowMapRenderTargetKey>();
                DO_ASSERT(parameters.shadow_rt != nullptr, "ShadowPass requires a ShadowMap RenderTargetHandle");

                RenderGraphAttachmentInfo depth_attach{};
                depth_attach.load_op = LoadOp::Clear;
                parameters.shadow_map = pass_builder.writeDepth(pass_builder.importTexture(
                    parameters.shadow_rt->getDepthTexture(), "ShadowMap"), depth_attach);
                parameters.primitive_scene_buffer = pass_builder.read(scene_textures->instance_scene_data);

                pass_builder.blackboard().set<ShadowMapKey>(parameters.shadow_map);
            },
            [this, processor = m_mesh_processor](const ShadowPassParameters& parameters, const RenderGraphPassContext& ctx, DrawCommandList& command_list) {
                DO_ASSERT(ctx.getView() != nullptr, "ShadowPass view is null");

                const auto* mesh_ext = ctx.getView()->getExtension<MeshViewExtension>();
                if (!mesh_ext) return;

                const auto viewport_state = rendering_pipeline_utils::BuildViewportState(*ctx.getView(), ctx.getGfxContext()->getSwapchainExtent2D());

                const auto resolved_psb = ctx.resolveBuffer(parameters.primitive_scene_buffer);

                const GlobalMeshShaderData global_data{mesh_ext->frame_time_data};
                command_list.writeBuffer(processor->getGlobalConstantBuffer(), &global_data, sizeof(global_data));
                const ViewMeshShaderData view_data{mesh_ext->directional_shadow_view_projection};
                command_list.writeBuffer(processor->getViewConstantBuffer(), &view_data, sizeof(view_data));

                auto* feature = static_cast<ShadowSceneFeature*>(m_owning_feature);
                const auto& draw_list = feature->getShadowDrawLists()[ctx.getViewIndex()];

                const auto fb = ctx.getFramebuffer();
                SubmitMeshDrawCommands(draw_list.cached_instances, *draw_list.cached_commands,
                    {}, {}, fb, viewport_state, resolved_psb, command_list);
                SubmitMeshDrawCommands(draw_list.dynamic_instances, draw_list.frame_commands,
                    {}, {}, fb, viewport_state, resolved_psb, command_list);
            }
        );
    }

} // namespace dodoe
