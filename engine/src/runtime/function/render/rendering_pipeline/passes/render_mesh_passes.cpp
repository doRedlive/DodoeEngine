#include "render_mesh_passes.h"
#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "render_pass_blackboard_keys.h"

#include "../render_view.h"
#include "../rendering_pipeline_pass_utils.h"

#include "runtime/function/render/mesh_draw/directional_shadow_mesh_processor.h"
#include "runtime/function/render/mesh_draw/gbuffer_mesh_processor.h"
#include "runtime/function/render/mesh_draw/mesh_draw_command_dispatcher.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"

namespace dodoe::RenderingPipelinePasses {
    namespace {

        struct GBufferPassParameters {
            RenderGraphTextureHandle albedo{};
            RenderGraphTextureHandle normal{};
            RenderGraphTextureHandle position{};
            RenderGraphTextureHandle material{};
            RenderGraphTextureHandle depth{};
            RenderGraphBufferHandle primitive_scene_buffer{};
            RenderGraphBufferHandle constant_buffer{};
        };

        struct ShadowPassParameters {
            RenderGraphTextureHandle shadow_map{};
            RenderGraphBufferHandle primitive_scene_buffer{};
            RenderGraphBufferHandle constant_buffer{};
        };

        void addGBufferPass(RenderGraphBuilder& graph, const RenderView& view, const RenderingPassContext& pass_context) {
            DO_ASSERT(pass_context.isValid(), "RenderingPipeline pass context is invalid");
            DO_ASSERT(pass_context.gbuffer_mesh_processor != nullptr, "RenderingPipeline GBuffer mesh processor is null");

            const auto& gbuffer_mesh_processor = *pass_context.gbuffer_mesh_processor;
            const Size_t visible_instance_count = view.instanceData().instance_scene_data.size();

            graph.addPass<GBufferPassParameters>(
                "GBufferPass",
                RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
                [pass_context, visible_instance_count, &gbuffer_mesh_processor](RenderGraphPassBuilder& pass_builder, GBufferPassParameters& parameters) {
                    const auto swapchain_extent = pass_context.gfx_context->getSwapchainExtent2d();
                    auto create_color_target = [swapchain_extent](const GfxFormat format, const char* debug_name) {
                        RenderGraphTextureDesc texture_desc{};
                        texture_desc.desc = GfxTextureDesc()
                            .setDimension(GfxTextureDimension::Texture2D)
                            .setWidth(static_cast<UInt32>(swapchain_extent.x))
                            .setHeight(static_cast<UInt32>(swapchain_extent.y))
                            .setFormat(format)
                            .setIsRenderTarget(true)
                            .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
                            .setDebugName(debug_name);
                        return texture_desc;
                    };

                    RenderGraphTextureDesc depth_desc{};
                    depth_desc.desc = GfxTextureDesc()
                        .setDimension(GfxTextureDimension::Texture2D)
                        .setWidth(static_cast<UInt32>(swapchain_extent.x))
                        .setHeight(static_cast<UInt32>(swapchain_extent.y))
                        .setFormat(GfxFormat::D32)
                        .setIsRenderTarget(true)
                        .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
                        .enableAutomaticStateTracking(GfxResourceStates::DepthWrite)
                        .setDebugName("RDG MainCameraDepth");

                    RenderGraphBufferDesc primitive_scene_buffer_desc{};
                    primitive_scene_buffer_desc.desc = GfxBufferDesc()
                        .setByteSize(static_cast<UInt32>(std::max<Size_t>(visible_instance_count, 1) * sizeof(InstanceSceneData)))
                        .setIsVertexBuffer(true)
                        .enableAutomaticStateTracking(GfxResourceStates::VertexBuffer)
                        .setDebugName("RDG MainCamera PrimitiveSceneBuffer");

                    parameters.albedo = pass_builder.write(pass_builder.createTransientTexture(create_color_target(GfxFormat::RGBA8_UNORM, "RDG MainCameraAlbedo"), "MainCameraAlbedo"));
                    parameters.normal = pass_builder.write(pass_builder.createTransientTexture(create_color_target(GfxFormat::RGBA16_FLOAT, "RDG MainCameraNormal"), "MainCameraNormal"));
                    parameters.position = pass_builder.write(pass_builder.createTransientTexture(create_color_target(GfxFormat::RGBA32_FLOAT, "RDG MainCameraPosition"), "MainCameraPosition"));
                    parameters.material = pass_builder.write(pass_builder.createTransientTexture(create_color_target(GfxFormat::RGBA8_UNORM, "RDG MainCameraMaterial"), "MainCameraMaterial"));
                    parameters.depth = pass_builder.write(pass_builder.createTransientTexture(depth_desc, "MainCameraDepth"));
                    parameters.primitive_scene_buffer = pass_builder.write(pass_builder.createTransientBuffer(primitive_scene_buffer_desc, "MainCameraPrimitiveSceneBuffer"));
                    parameters.constant_buffer = pass_builder.importBuffer(gbuffer_mesh_processor.getConstantBuffer(), "GBufferConstantBuffer");

                    pass_builder.blackboard().set<GBufferAlbedoKey>(parameters.albedo);
                    pass_builder.blackboard().set<GBufferNormalKey>(parameters.normal);
                    pass_builder.blackboard().set<GBufferPositionKey>(parameters.position);
                    pass_builder.blackboard().set<GBufferMaterialKey>(parameters.material);
                    pass_builder.blackboard().set<GBufferDepthKey>(parameters.depth);
                    pass_builder.blackboard().set<InstanceBufferKey>(parameters.primitive_scene_buffer);
                },
                [&gbuffer_mesh_processor](const GBufferPassParameters& parameters, const RenderGraphPassContext& context, RenderGraphCommandList& command_list) {
                    DO_ASSERT(context.getView() != nullptr, "GBufferPass view is null");
                    const auto* view = context.getView();

                    const auto device = context.getGfxContext()->getDevice();
                    const auto albedo = command_list.resolveTexture(parameters.albedo);
                    const auto normal = command_list.resolveTexture(parameters.normal);
                    const auto position = command_list.resolveTexture(parameters.position);
                    const auto material = command_list.resolveTexture(parameters.material);
                    const auto depth = command_list.resolveTexture(parameters.depth);
                    const auto primitive_scene_buffer = command_list.resolveBuffer(parameters.primitive_scene_buffer);
                    auto framebuffer = device->createFramebuffer(
                        GfxFramebufferDesc()
                            .addColorAttachment(albedo)
                            .addColorAttachment(normal)
                            .addColorAttachment(position)
                            .addColorAttachment(material)
                            .setDepthAttachment(depth)
                    );
                    const auto viewport_state = rendering_pipeline_utils::BuildViewportState(*context.getView(), context.getGfxContext()->getSwapchainExtent2d());

                    command_list.open();
                    command_list.beginMarker("GBufferPass");
                    command_list.setTextureState(parameters.albedo, GfxAllSubresources, GfxResourceStates::RenderTarget);
                    command_list.setTextureState(parameters.normal, GfxAllSubresources, GfxResourceStates::RenderTarget);
                    command_list.setTextureState(parameters.position, GfxAllSubresources, GfxResourceStates::RenderTarget);
                    command_list.setTextureState(parameters.material, GfxAllSubresources, GfxResourceStates::RenderTarget);
                    command_list.setTextureState(parameters.depth, GfxAllSubresources, GfxResourceStates::DepthWrite);
                    command_list.commitBarriers();
                    command_list.clearTextureFloat(parameters.albedo, GfxAllSubresources, GfxColor(0.08f, 0.09f, 0.11f, 1.0f));
                    command_list.clearTextureFloat(parameters.normal, GfxAllSubresources, GfxColor(0.0f, 0.0f, 0.0f, 1.0f));
                    command_list.clearTextureFloat(parameters.position, GfxAllSubresources, GfxColor(0.0f, 0.0f, 0.0f, 1.0f));
                    command_list.clearTextureFloat(parameters.material, GfxAllSubresources, GfxColor(0.0f, 1.0f, 1.0f, 1.0f));
                    command_list.clearDepthStencilTexture(parameters.depth, GfxAllSubresources, true, 1.0f, false, 0);
                    command_list.setBufferState(parameters.primitive_scene_buffer, GfxResourceStates::CopyDest);
                    command_list.commitBarriers();
                    MeshDrawCommandDispatcher::uploadInstanceTransforms(view->instanceData(), parameters.primitive_scene_buffer, command_list);
                    command_list.setBufferState(parameters.primitive_scene_buffer, GfxResourceStates::VertexBuffer);
                    command_list.commitBarriers();
                    MeshDrawCommandDispatcher::dispatch(
                        MeshPassType::GBuffer,
                        view->shaderData(),
                        view->passData(),
                        view->passData().getMeshPassCommands(MeshPassType::GBuffer),
                        framebuffer,
                        viewport_state,
                        GfxGraphicsPipelineHandle{},
                        parameters.primitive_scene_buffer,
                        parameters.constant_buffer,
                        command_list
                    );
                    command_list.setTextureState(parameters.albedo, GfxAllSubresources, GfxResourceStates::ShaderResource);
                    command_list.setTextureState(parameters.normal, GfxAllSubresources, GfxResourceStates::ShaderResource);
                    command_list.setTextureState(parameters.position, GfxAllSubresources, GfxResourceStates::ShaderResource);
                    command_list.setTextureState(parameters.material, GfxAllSubresources, GfxResourceStates::ShaderResource);
                    command_list.setTextureState(parameters.depth, GfxAllSubresources, GfxResourceStates::ShaderResource);
                    command_list.commitBarriers();
                    command_list.endMarker();
                    command_list.close();
                }
            );
        }

        void addDirectionalShadowPass(RenderGraphBuilder& graph, const RenderingPassContext& pass_context) {
            DO_ASSERT(pass_context.isValid(), "RenderingPipeline pass context is invalid");
            DO_ASSERT(pass_context.directional_shadow_mesh_processor != nullptr, "RenderingPipeline directional shadow mesh processor is null");

            const auto& directional_shadow_mesh_processor = *pass_context.directional_shadow_mesh_processor;

            graph.addPass<ShadowPassParameters>(
                "DirectionalShadowPass",
                RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
                [pass_context, &directional_shadow_mesh_processor](RenderGraphPassBuilder& pass_builder, ShadowPassParameters& parameters) {
                    const auto swapchain_extent = pass_context.gfx_context->getSwapchainExtent2d();
                    const auto* primitive_scene_buffer = pass_builder.blackboard().get<InstanceBufferKey, RenderGraphBufferHandle>();
                    DO_ASSERT(primitive_scene_buffer, "DirectionalShadowPass primitive scene buffer is missing");

                    RenderGraphTextureDesc shadow_desc{};
                    shadow_desc.desc = GfxTextureDesc()
                        .setDimension(GfxTextureDimension::Texture2D)
                        .setWidth(static_cast<UInt32>(swapchain_extent.x))
                        .setHeight(static_cast<UInt32>(swapchain_extent.y))
                        .setFormat(GfxFormat::D32)
                        .setIsRenderTarget(true)
                        .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
                        .enableAutomaticStateTracking(GfxResourceStates::DepthWrite)
                        .setDebugName("RDG ShadowMap");

                    parameters.shadow_map = pass_builder.write(pass_builder.createTransientTexture(shadow_desc, "ShadowMap"));
                    parameters.primitive_scene_buffer = pass_builder.read(*primitive_scene_buffer);
                    parameters.constant_buffer = pass_builder.importBuffer(directional_shadow_mesh_processor.getConstantBuffer(), "DirectionalShadowConstantBuffer");
                    pass_builder.blackboard().set<ShadowMapKey>(parameters.shadow_map);
                },
                [&directional_shadow_mesh_processor](const ShadowPassParameters& parameters, const RenderGraphPassContext& context, RenderGraphCommandList& command_list) {
                    DO_ASSERT(context.getView() != nullptr, "DirectionalShadowPass view is null");
                    const auto* view = context.getView();

                    const auto device = context.getGfxContext()->getDevice();
                    const auto shadow_map = command_list.resolveTexture(parameters.shadow_map);
                    auto framebuffer = device->createFramebuffer(GfxFramebufferDesc().setDepthAttachment(shadow_map));
                    const auto viewport_state = rendering_pipeline_utils::BuildViewportState(*context.getView(), context.getGfxContext()->getSwapchainExtent2d());

                    command_list.open();
                    command_list.beginMarker("DirectionalShadowPass");
                    command_list.setTextureState(parameters.shadow_map, GfxAllSubresources, GfxResourceStates::DepthWrite);
                    command_list.commitBarriers();
                    command_list.clearDepthStencilTexture(parameters.shadow_map, GfxAllSubresources, true, 1.0f, false, 0);
                    MeshDrawCommandDispatcher::dispatch(
                        MeshPassType::DirectionalShadow,
                        view->shaderData(),
                        view->passData(),
                        view->passData().getMeshPassCommands(MeshPassType::DirectionalShadow),
                        framebuffer,
                        viewport_state,
                        GfxGraphicsPipelineHandle{},
                        parameters.primitive_scene_buffer,
                        parameters.constant_buffer,
                        command_list
                    );
                    command_list.setTextureState(parameters.shadow_map, GfxAllSubresources, GfxResourceStates::ShaderResource);
                    command_list.commitBarriers();
                    command_list.endMarker();
                    command_list.close();
                }
            );
        }
    } // namespace

    void AddMeshGraphPasses(RenderGraphBuilder& graph, const RenderView& view, const RenderingPassContext& pass_context) {
        addGBufferPass(graph, view, pass_context);
        addDirectionalShadowPass(graph, pass_context);
    }

} // namespace dodoe::RenderingPipelinePasses
