// do@Redlive

#include "render_base_pass.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "render_pass_blackboard_keys.h"

#include "../../render_view/render_view.h"
#include "../../render_view/mesh_view_extension.h"
#include "../render_pipeline_pass_utils.h"
#include "runtime/function/render/render_settings.h"

#include "runtime/function/render/mesh_draw/gbuffer_mesh_processor.h"
#include "runtime/function/render/mesh_draw/directional_shadow_mesh_processor.h"
#include "runtime/function/render/mesh_draw/mesh_draw_list.h"
#include "runtime/function/render/mesh_draw/mesh_processor_base.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_service/render_target_handle.h"
#include "runtime/function/render/render_pipeline/render_feature/base_scene_feature.h"
#include "../resource_registry.h"

namespace dodoe {

    struct GBufferPassParameters {
        RenderGraphTextureHandle albedo{};
        RenderGraphTextureHandle normal{};
        RenderGraphTextureHandle position{};
        RenderGraphTextureHandle material{};
        RenderGraphTextureHandle depth{};
        RenderGraphBufferHandle primitive_scene_buffer{};
        RenderGraphBufferHandle constant_buffer{};
        RenderTargetHandle* gbuffer_rt{nullptr};
    };

    void GBufferPass::build(RenderGraphBuilder& graph,
                             const RenderPassBuildContext& context) {
        DO_ASSERT(m_mesh_processor != nullptr, "GBufferPass requires mesh processor");

        graph.addPass<GBufferPassParameters>(
            "GBufferPass",
            RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
            [view = &context.view, registry = context.resource_registry, processor = m_mesh_processor]
            (RenderGraphPassBuilder& b, GBufferPassParameters& p) {
                const auto* mesh_ext = view->getExtension<MeshViewExtension>();
                const Size_t visible_instance_count = mesh_ext ? mesh_ext->instance_scene_data.size() : 0;

                p.gbuffer_rt = registry->findRenderTarget("GBuffer");
                DO_ASSERT(p.gbuffer_rt != nullptr, "GBufferPass requires GBuffer RenderTargetHandle from BaseSceneFeature");

                p.albedo   = b.writeColor(b.importTexture(p.gbuffer_rt->getColorTexture(0), "BaseAlbedo"));
                p.normal   = b.writeColor(b.importTexture(p.gbuffer_rt->getColorTexture(1), "BaseNormal"));
                p.position = b.writeColor(b.importTexture(p.gbuffer_rt->getColorTexture(2), "BasePosition"));
                p.material = b.writeColor(b.importTexture(p.gbuffer_rt->getColorTexture(3), "BaseMaterial"));
                p.depth    = b.writeDepth(b.importTexture(p.gbuffer_rt->getDepthTexture(), "BaseDepth"));

                RenderGraphBufferDesc primitive_scene_buffer_desc{};
                primitive_scene_buffer_desc.desc = GfxBufferDesc()
                    .setByteSize(static_cast<UInt32>(std::max<Size_t>(visible_instance_count, 1) * sizeof(InstanceSceneData)))
                    .setIsVertexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::VertexBuffer)
                    .setDebugName("RDG BasePass PrimitiveSceneBuffer");
                p.primitive_scene_buffer = b.write(b.createTransientBuffer(primitive_scene_buffer_desc, "BasePrimitiveSceneBuffer"));

                p.constant_buffer = b.importBuffer(processor->getConstantBuffer(), "GBufferConstantBuffer");

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
                DO_ASSERT(ctx.getView() != nullptr, "BasePass view is null");

                const auto albedo = ctx.resolveTexture(p.albedo);
                const auto normal = ctx.resolveTexture(p.normal);
                const auto position = ctx.resolveTexture(p.position);
                const auto material = ctx.resolveTexture(p.material);
                const auto depth = ctx.resolveTexture(p.depth);

                auto framebuffer = p.gbuffer_rt->getFramebuffer(command_list);

                const auto viewport_state = rendering_pipeline_utils::BuildViewportState(*ctx.getView(), ctx.getGfxContext()->getSwapchainExtent2d());

                command_list.setTextureState(albedo, GfxAllSubresources, GfxResourceStates::RenderTarget);
                command_list.setTextureState(normal, GfxAllSubresources, GfxResourceStates::RenderTarget);
                command_list.setTextureState(position, GfxAllSubresources, GfxResourceStates::RenderTarget);
                command_list.setTextureState(material, GfxAllSubresources, GfxResourceStates::RenderTarget);
                command_list.setTextureState(depth, GfxAllSubresources, GfxResourceStates::DepthWrite);
                command_list.commitBarriers();
                command_list.clearTextureFloat(albedo, GfxAllSubresources, GfxColor(0.08f, 0.09f, 0.11f, 1.0f));
                command_list.clearTextureFloat(normal, GfxAllSubresources, GfxColor(0.0f, 0.0f, 0.0f, 1.0f));
                command_list.clearTextureFloat(position, GfxAllSubresources, GfxColor(0.0f, 0.0f, 0.0f, 1.0f));
                command_list.clearTextureFloat(material, GfxAllSubresources, GfxColor(0.0f, 1.0f, 1.0f, 1.0f));
                command_list.clearDepthStencilTexture(depth, GfxAllSubresources, true, 1.0f, false, 0);

                const auto* mesh_ext = ctx.getView()->getExtension<MeshViewExtension>();
                const auto& instance_data = mesh_ext ? mesh_ext->instance_scene_data : DynamicArray<InstanceSceneData>{};
                const auto resolved_psb = ctx.resolveBuffer(p.primitive_scene_buffer);
                command_list.setBufferState(resolved_psb, GfxResourceStates::CopyDest);
                command_list.commitBarriers();
                command_list.writeBuffer(resolved_psb, instance_data.data(), instance_data.size() * sizeof(InstanceSceneData));
                command_list.setBufferState(resolved_psb, GfxResourceStates::VertexBuffer);

                auto* feature = static_cast<BaseSceneFeature*>(m_owning_feature);
                DO_ASSERT(feature != nullptr, "GBufferPass owning feature is null");
                const auto& draw_list = feature->getGBufferDrawLists()[ctx.getViewIndex()];

                DynamicArray<GfxBindingSetHandle> extra_bindings;
                if (RenderSettings::IsBindlessActive() && processor->getDescriptorBindingSet()) {
                    extra_bindings.push_back(processor->getDescriptorBindingSet());
                }

                SubmitMeshDrawCommands(draw_list.cached_instances, *draw_list.cached_commands,
                    framebuffer, viewport_state, resolved_psb, extra_bindings, command_list);
                SubmitMeshDrawCommands(draw_list.dynamic_instances, draw_list.frame_commands,
                    framebuffer, viewport_state, resolved_psb, extra_bindings, command_list);

                command_list.setTextureState(albedo, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.setTextureState(normal, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.setTextureState(position, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.setTextureState(material, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.setTextureState(depth, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.commitBarriers();
            }
        );
    }

    struct ShadowPassParameters {
        RenderGraphTextureHandle shadow_map{};
        RenderGraphBufferHandle primitive_scene_buffer{};
        RenderGraphBufferHandle constant_buffer{};
        RenderTargetHandle* shadow_rt{nullptr};
    };

    void DirectionalShadowPass::build(RenderGraphBuilder& graph,
                                       const RenderPassBuildContext& context) {
        DO_ASSERT(m_mesh_processor != nullptr, "DirectionalShadowPass requires mesh processor");

        graph.addPass<ShadowPassParameters>(
            "DirectionalShadowPass",
            RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
            [view = &context.view, registry = context.resource_registry, processor = m_mesh_processor]
            (RenderGraphPassBuilder& pass_builder, ShadowPassParameters& parameters) {
                const auto* scene_textures = pass_builder.blackboard().get<SceneTexturesKey, SceneTextures>();
                DO_ASSERT(scene_textures, "DirectionalShadowPass scene textures are missing");

                parameters.shadow_rt = registry ? registry->findRenderTarget("ShadowMap") : nullptr;
                DO_ASSERT(parameters.shadow_rt != nullptr, "DirectionalShadowPass requires ShadowMap RenderTargetHandle from BaseSceneFeature");

                parameters.shadow_map = pass_builder.writeDepth(pass_builder.importTexture(
                    parameters.shadow_rt->getDepthTexture(), "ShadowMap"));
                parameters.primitive_scene_buffer = pass_builder.read(scene_textures->instance_scene_data);

                parameters.constant_buffer = pass_builder.importBuffer(processor->getConstantBuffer(), "DirectionalShadowConstantBuffer");
                pass_builder.blackboard().set<ShadowMapKey>(parameters.shadow_map);
            },
            [this, processor = m_mesh_processor](const ShadowPassParameters& parameters, const RenderGraphPassContext& ctx, DrawCommandList& command_list) {
                DO_ASSERT(ctx.getView() != nullptr, "DirectionalShadowPass view is null");

                const auto* mesh_ext = ctx.getView()->getExtension<MeshViewExtension>();
                if (!mesh_ext) return;

                const auto shadow_map = ctx.resolveTexture(parameters.shadow_map);
                auto framebuffer = parameters.shadow_rt->getFramebuffer(command_list);

                const auto viewport_state = rendering_pipeline_utils::BuildViewportState(*ctx.getView(), ctx.getGfxContext()->getSwapchainExtent2d());

                command_list.setTextureState(shadow_map, GfxAllSubresources, GfxResourceStates::DepthWrite);
                command_list.commitBarriers();
                command_list.clearDepthStencilTexture(shadow_map, GfxAllSubresources, true, 1.0f, false, 0);

                const auto resolved_psb = ctx.resolveBuffer(parameters.primitive_scene_buffer);

                auto* feature = static_cast<BaseSceneFeature*>(m_owning_feature);
                const auto& draw_list = feature->getShadowDrawLists()[ctx.getViewIndex()];

                SubmitMeshDrawCommands(draw_list.cached_instances, *draw_list.cached_commands,
                    framebuffer, viewport_state, resolved_psb, {}, command_list);
                SubmitMeshDrawCommands(draw_list.dynamic_instances, draw_list.frame_commands,
                    framebuffer, viewport_state, resolved_psb, {}, command_list);

                command_list.setTextureState(shadow_map, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.commitBarriers();
            }
        );
    }

} // namespace dodoe
