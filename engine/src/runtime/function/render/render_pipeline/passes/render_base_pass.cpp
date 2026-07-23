// do@Redlive

#include "render_base_pass.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "render_pass_blackboard_keys.h"

#include "../../render_view/render_view.h"
#include "../../render_view/mesh_view_extension.h"
#include "../render_pipeline_pass_utils.h"

#include "runtime/function/render/mesh_draw/gbuffer_mesh_processor.h"
#include "runtime/function/render/mesh_draw/directional_shadow_mesh_processor.h"
#include "runtime/function/render/mesh_draw/mesh_draw_command_dispatcher.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"

namespace dodoe {

    struct GBufferPassParameters {
        RenderGraphTextureHandle albedo{};
        RenderGraphTextureHandle normal{};
        RenderGraphTextureHandle position{};
        RenderGraphTextureHandle material{};
        RenderGraphTextureHandle depth{};
        RenderGraphBufferHandle primitive_scene_buffer{};
        RenderGraphBufferHandle constant_buffer{};
    };

    void GBufferPass::build(RenderGraphBuilder& graph,
                             const RenderPassBuildContext& context) {
        const auto& pass_context = context.pass_context;
        DO_ASSERT(pass_context.isValid(), "RenderPipeline pass context is invalid");

        graph.addPass<GBufferPassParameters>(
            "GBufferPass",
            RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
            [pass_context, view = &context.view](RenderGraphPassBuilder& b, GBufferPassParameters& p) {
                const auto* mesh_ext = view->getExtension<MeshViewExtension>();
                const Size_t visible_instance_count = mesh_ext ? mesh_ext->instance_scene_data.size() : 0;

                const auto swapchain_extent = pass_context.gfx_context->getSwapchainExtent2d();
                using namespace rendering_pipeline_utils;

                p.albedo   = b.write(b.createTransientTexture(MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA8_UNORM,  "RDG BaseAlbedo"),   "BaseAlbedo"));
                p.normal   = b.write(b.createTransientTexture(MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA16_FLOAT, "RDG BaseNormal"),   "BaseNormal"));
                p.position = b.write(b.createTransientTexture(MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA32_FLOAT, "RDG BasePosition"), "BasePosition"));
                p.material = b.write(b.createTransientTexture(MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA8_UNORM,  "RDG BaseMaterial"), "BaseMaterial"));
                p.depth    = b.write(b.createTransientTexture(MakeSwapchainDepth2D(swapchain_extent, GfxFormat::D32, "RDG BaseDepth"), "BaseDepth"));

                RenderGraphBufferDesc primitive_scene_buffer_desc{};
                primitive_scene_buffer_desc.desc = GfxBufferDesc()
                    .setByteSize(static_cast<UInt32>(std::max<Size_t>(visible_instance_count, 1) * sizeof(InstanceSceneData)))
                    .setIsVertexBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::VertexBuffer)
                    .setDebugName("RDG BasePass PrimitiveSceneBuffer");
                p.primitive_scene_buffer = b.write(b.createTransientBuffer(primitive_scene_buffer_desc, "BasePrimitiveSceneBuffer"));

                const auto& mesh_processor = pass_context.getMeshProcessor<MeshPassType::GBuffer>();
                p.constant_buffer = b.importBuffer(mesh_processor.getConstantBuffer(), "GBufferConstantBuffer");

                SceneTextures gbuffer;
                gbuffer.albedo   = p.albedo;
                gbuffer.normal   = p.normal;
                gbuffer.position = p.position;
                gbuffer.material = p.material;
                gbuffer.depth    = p.depth;
                gbuffer.instance_scene_data = p.primitive_scene_buffer;
                b.blackboard().set<SceneTexturesKey>(gbuffer);
            },
            [pass_context](const GBufferPassParameters& p, const RenderGraphPassContext& ctx, DrawCommandList& command_list) {
                DO_ASSERT(ctx.getView() != nullptr, "BasePass view is null");

                const auto* view = ctx.getView();

                const auto albedo = ctx.resolveTexture(p.albedo);
                const auto normal = ctx.resolveTexture(p.normal);
                const auto position = ctx.resolveTexture(p.position);
                const auto material = ctx.resolveTexture(p.material);
                const auto depth = ctx.resolveTexture(p.depth);
                const auto primitive_scene_buffer = ctx.resolveBuffer(p.primitive_scene_buffer);

                auto framebuffer_desc = GfxFramebufferDesc()
                    .addColorAttachment(albedo)
                    .addColorAttachment(normal)
                    .addColorAttachment(position)
                    .addColorAttachment(material)
                    .setDepthAttachment(depth);
                auto framebuffer = command_list.createFramebuffer(framebuffer_desc);

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
                command_list.setBufferState(primitive_scene_buffer, GfxResourceStates::CopyDest);
                command_list.commitBarriers();

                const auto* mesh_ext = view->getExtension<MeshViewExtension>();
                if (mesh_ext) {
                    MeshDrawCommandDispatcher::UploadInstanceTransforms(ctx, mesh_ext->instance_scene_data, p.primitive_scene_buffer, command_list);
                    command_list.setBufferState(primitive_scene_buffer, GfxResourceStates::VertexBuffer);
                    command_list.commitBarriers();
                    MeshDrawCommandDispatcher::DispatchCached(
                        ctx,
                        MeshPassType::GBuffer,
                        mesh_ext->gbuffer_shader_data,
                        mesh_ext->cached_draw_instances[static_cast<size_t>(MeshPassType::GBuffer)],
                        *mesh_ext->cached_commands,
                        framebuffer,
                        viewport_state,
                        GfxGraphicsPipelineHandle{},
                        p.primitive_scene_buffer,
                        p.constant_buffer,
                        command_list
                    );
                    MeshDrawCommandDispatcher::DispatchCached(
                        ctx,
                        MeshPassType::GBuffer,
                        mesh_ext->dynamic_shader_data,
                        mesh_ext->dynamic_draw_instances[static_cast<size_t>(MeshPassType::GBuffer)],
                        mesh_ext->frame_commands,
                        framebuffer,
                        viewport_state,
                        GfxGraphicsPipelineHandle{},
                        p.primitive_scene_buffer,
                        p.constant_buffer,
                        command_list
                    );
                }

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
    };

    void DirectionalShadowPass::build(RenderGraphBuilder& graph,
                                       const RenderPassBuildContext& context) {
        const auto& pass_context = context.pass_context;
        DO_ASSERT(pass_context.isValid(), "RenderingPipeline pass context is invalid");

        graph.addPass<ShadowPassParameters>(
            "DirectionalShadowPass",
            RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
            [pass_context, view = &context.view](RenderGraphPassBuilder& pass_builder, ShadowPassParameters& parameters) {
                const auto swapchain_extent = pass_context.gfx_context->getSwapchainExtent2d();
                const auto* scene_textures = pass_builder.blackboard().get<SceneTexturesKey, SceneTextures>();
                DO_ASSERT(scene_textures, "DirectionalShadowPass scene textures are missing");

                parameters.shadow_map = pass_builder.write(pass_builder.createTransientTexture(
                    rendering_pipeline_utils::MakeSwapchainDepth2D(swapchain_extent, GfxFormat::D32, "RDG ShadowMap"),
                    "ShadowMap"));
                parameters.primitive_scene_buffer = pass_builder.read(scene_textures->instance_scene_data);

                const auto& directional_shadow_mesh_processor = pass_context.getMeshProcessor<MeshPassType::DirectionalShadow>();
                parameters.constant_buffer = pass_builder.importBuffer(directional_shadow_mesh_processor.getConstantBuffer(), "DirectionalShadowConstantBuffer");
                pass_builder.blackboard().set<ShadowMapKey>(parameters.shadow_map);
            },
            [pass_context, view = &context.view](const ShadowPassParameters& parameters, const RenderGraphPassContext& ctx, DrawCommandList& command_list) {
                DO_ASSERT(ctx.getView() != nullptr, "DirectionalShadowPass view is null");

                const auto* mesh_ext = view->getExtension<MeshViewExtension>();
                if (!mesh_ext) {
                    return;
                }

                const auto shadow_map = ctx.resolveTexture(parameters.shadow_map);

                auto framebuffer_desc = GfxFramebufferDesc().setDepthAttachment(shadow_map);
                auto framebuffer = command_list.createFramebuffer(framebuffer_desc);

                const auto viewport_state = rendering_pipeline_utils::BuildViewportState(*ctx.getView(), ctx.getGfxContext()->getSwapchainExtent2d());

                command_list.setTextureState(shadow_map, GfxAllSubresources, GfxResourceStates::DepthWrite);
                command_list.commitBarriers();
                command_list.clearDepthStencilTexture(shadow_map, GfxAllSubresources, true, 1.0f, false, 0);

                const auto shadow_cb = ctx.resolveBuffer(parameters.constant_buffer);
                struct ShadowConstantData {
                    Matrix4f light_view_projection;
                    Vector4f time_data;
                };
                const ShadowConstantData shadow_data{
                    mesh_ext->directional_shadow_view_projection,
                    mesh_ext->frame_time_data
                };
                command_list.setBufferState(shadow_cb, GfxResourceStates::CopyDest);
                command_list.commitBarriers();
                command_list.writeBuffer(shadow_cb, &shadow_data, sizeof(shadow_data));
                command_list.setBufferState(shadow_cb, GfxResourceStates::ConstantBuffer);
                command_list.commitBarriers();

                MeshDrawCommandDispatcher::DispatchCached(
                    ctx,
                    MeshPassType::DirectionalShadow,
                    mesh_ext->gbuffer_shader_data,
                    mesh_ext->cached_draw_instances[static_cast<size_t>(MeshPassType::DirectionalShadow)],
                    *mesh_ext->cached_commands,
                    framebuffer,
                    viewport_state,
                    GfxGraphicsPipelineHandle{},
                    parameters.primitive_scene_buffer,
                    parameters.constant_buffer,
                    command_list
                );
                MeshDrawCommandDispatcher::DispatchCached(
                    ctx,
                    MeshPassType::DirectionalShadow,
                    mesh_ext->dynamic_shader_data,
                    mesh_ext->dynamic_draw_instances[static_cast<size_t>(MeshPassType::DirectionalShadow)],
                    mesh_ext->frame_commands,
                    framebuffer,
                    viewport_state,
                    GfxGraphicsPipelineHandle{},
                    parameters.primitive_scene_buffer,
                    parameters.constant_buffer,
                    command_list
                );
                command_list.setTextureState(shadow_map, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.commitBarriers();
            }
        );
    }

} // namespace dodoe
