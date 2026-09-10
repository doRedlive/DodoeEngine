// do@Redlive

#include "runtime/function/render/render_pipeline/passes/render_shadow_pass.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "runtime/function/render/render_pipeline/passes/render_pass_blackboard_keys.h"

#include "runtime/function/render/render_view/render_view.h"
#include "runtime/function/render/render_view/mesh_view_extension.h"
#include "runtime/function/render/render_view/shadow_view_extension.h"
#include "runtime/function/render/render_pipeline/render_pipeline_pass_utils.h"

#include "runtime/function/render/mesh_draw/shadow_mesh_processor.h"
#include "runtime/function/render/mesh_draw/mesh_draw_list.h"
#include "runtime/function/render/mesh_draw/mesh_processor_base.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_service/render_target_handle.h"
#include "runtime/function/render/render_pipeline/render_feature/shadow_scene_feature.h"
#include "runtime/function/render/render_pipeline/render_graph_import_keys.h"

namespace dodoe {

    namespace {
        GfxViewportState MakeCascadeViewport(UInt32 cascade,
                                            UInt32 atlas_width, UInt32 atlas_height) {
            const UInt32 column = cascade % 2u;
            const UInt32 row = cascade / 2u;
            const UInt32 quad_width = atlas_width / 2u;
            const UInt32 quad_height = atlas_height / 2u;
            const Float x0 = static_cast<Float>(column * quad_width);
            const Float y0 = static_cast<Float>(row * quad_height);
            const Float width = static_cast<Float>(column == 0u ? quad_width : atlas_width - quad_width);
            const Float height = static_cast<Float>(row == 0u ? quad_height : atlas_height - quad_height);
            return GfxViewportState().addViewportAndScissorRect(
                GfxViewport(x0, x0 + width, y0, y0 + height, 0.0f, 1.0f));
        }
    }

    struct ShadowPassParameters {
        RenderGraphTextureHandle shadow_map{};
        RenderGraphBufferHandle caster_instance_buffer{};
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
                DO_ASSERT(imports != nullptr, "ShadowPass graph imports are null");
                parameters.shadow_rt = imports->require<ShadowMapRenderTargetKey>();
                DO_ASSERT(parameters.shadow_rt != nullptr, "ShadowPass requires a ShadowMap RenderTargetHandle");

                RenderGraphAttachmentInfo depth_attach{};
                depth_attach.load_op = LoadOp::Clear;
                parameters.shadow_map = pass_builder.writeDepth(pass_builder.importTexture(
                    parameters.shadow_rt->getDepthTexture(), "ShadowMap"), depth_attach);

                const auto* shadow_ext = view->getExtension<ShadowViewExtension>();
                const Size_t caster_instance_count = shadow_ext
                    ? shadow_ext->getData().shadow_caster_instance_data.size() : 0;

                RenderGraphBufferDesc caster_instance_desc{};
                caster_instance_desc.desc = GfxBufferDesc()
                    .setByteSize(static_cast<UInt32>(std::max<Size_t>(caster_instance_count, 1) * sizeof(InstanceSceneData)))
                    .setIsVertexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::VertexBuffer)
                    .setDebugName("RDG ShadowPass CasterInstanceBuffer");
                parameters.caster_instance_buffer = pass_builder.write(pass_builder.createTransientBuffer(
                    caster_instance_desc, "ShadowCasterInstanceBuffer"));
                pass_builder.read(parameters.caster_instance_buffer);

                pass_builder.blackboard().set<ShadowMapKey>(parameters.shadow_map);
            },
            [this, processor = m_mesh_processor](const ShadowPassParameters& parameters, const RenderGraphPassContext& ctx, DrawCommandList& command_list) {
                DO_ASSERT(ctx.getView() != nullptr, "ShadowPass view is null");

                const auto* shadow_ext = ctx.getView()->getExtension<ShadowViewExtension>();
                if (!shadow_ext || !shadow_ext->getData().has_shadow) return;
                const auto* mesh_ext = ctx.getView()->getExtension<MeshViewExtension>();

                const auto shadow_width = parameters.shadow_rt->getWidth();
                const auto shadow_height = parameters.shadow_rt->getHeight();

                const auto resolved_instances = ctx.resolveBuffer(parameters.caster_instance_buffer);
                command_list.setBufferState(resolved_instances, GfxResourceStates::CopyDest);
                command_list.commitBarriers();
                command_list.writeBuffer(resolved_instances,
                    shadow_ext->getData().shadow_caster_instance_data.data(),
                    shadow_ext->getData().shadow_caster_instance_data.size() * sizeof(InstanceSceneData));
                command_list.setBufferState(resolved_instances, GfxResourceStates::VertexBuffer);

                const auto global_data = GlobalMeshShaderData{
                    mesh_ext ? mesh_ext->frame_time_data : Vector4f(0.0f)};
                command_list.writeBuffer(processor->getGlobalConstantBuffer(), &global_data, sizeof(global_data));
                const auto fb = ctx.getFramebuffer();
                auto* feature = static_cast<ShadowSceneFeature*>(m_owning_feature);
                const auto& draw_lists = feature->getShadowDrawLists();
                if (ctx.getViewIndex() >= draw_lists.size()) {
                    DO_ERROR("ShadowPass draw list is missing for view {}", ctx.getViewIndex());
                    return;
                }
                const auto& draw_list = draw_lists[ctx.getViewIndex()];

                for (UInt32 cascade = 0; cascade < kShadowCascadeCount; ++cascade) {
                    const ViewMeshShaderData view_data{
                        shadow_ext->getData().cascade_view_projections[cascade]};
                    command_list.writeBuffer(processor->getViewConstantBuffer(), &view_data, sizeof(view_data));

                    const auto viewport_state = MakeCascadeViewport(
                        cascade, shadow_width, shadow_height);
                    SubmitMeshDrawSources(draw_list.sources, {}, fb, viewport_state,
                        resolved_instances, nullptr, command_list,
                        static_cast<UInt8>(1u << cascade));
                }
            }
        );
    }

} // namespace dodoe
