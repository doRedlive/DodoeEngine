#include "render_mesh_passes.h"

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
        };

        struct ShadowPassParameters {
            RenderGraphTextureHandle shadow_map{};
            RenderGraphBufferHandle primitive_scene_buffer{};
        };

        void addGBufferPass(RenderGraphBuilder& graph, const RenderView& view, const RenderingPassContext& pass_context) {
            DO_ASSERT(pass_context.isValid(), "RenderingPipeline pass context is invalid");
            DO_ASSERT(pass_context.gbuffer_mesh_processor != nullptr, "RenderingPipeline GBuffer mesh processor is null");

            const auto& gbuffer_mesh_processor = *pass_context.gbuffer_mesh_processor;
            const Size_t visible_instance_count = view.getMeshDrawContext().instance_scene_data.size();

            graph.addPass<GBufferPassParameters>(
                "GBufferPass",
                RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
                [pass_context, visible_instance_count](RenderGraphPassBuilder& pass_builder, GBufferPassParameters& parameters) {
                    const auto swapchain_extent = pass_context.gfx_context->getSwapchainExtent2d();
                    auto create_color_target = [swapchain_extent](const Format format, const char* debug_name) {
                        RenderGraphTextureDesc texture_desc{};
                        texture_desc.desc = TextureDesc()
                            .setDimension(TextureDimension::Texture2D)
                            .setWidth(static_cast<UInt32>(swapchain_extent.x))
                            .setHeight(static_cast<UInt32>(swapchain_extent.y))
                            .setFormat(format)
                            .setIsRenderTarget(true)
                            .enableAutomaticStateTracking(ResourceStates::ShaderResource)
                            .setDebugName(debug_name);
                        return texture_desc;
                    };

                    RenderGraphTextureDesc depth_desc{};
                    depth_desc.desc = TextureDesc()
                        .setDimension(TextureDimension::Texture2D)
                        .setWidth(static_cast<UInt32>(swapchain_extent.x))
                        .setHeight(static_cast<UInt32>(swapchain_extent.y))
                        .setFormat(Format::D32)
                        .setIsRenderTarget(true)
                        .setIsShaderResource(true)
                        .enableAutomaticStateTracking(ResourceStates::DepthWrite)
                        .setDebugName("RDG MainCameraDepth");

                    RenderGraphBufferDesc primitive_scene_buffer_desc{};
                    primitive_scene_buffer_desc.desc = BufferDesc()
                        .setByteSize(static_cast<UInt32>(std::max<Size_t>(visible_instance_count, 1) * sizeof(InstanceSceneData)))
                        .setIsVertexBuffer(true)
                        .enableAutomaticStateTracking(ResourceStates::VertexBuffer)
                        .setDebugName("RDG MainCamera PrimitiveSceneBuffer");

                    parameters.albedo = pass_builder.write(pass_builder.createTransientTexture(create_color_target(Format::RGBA8_UNORM, "RDG MainCameraAlbedo"), "MainCameraAlbedo"));
                    parameters.normal = pass_builder.write(pass_builder.createTransientTexture(create_color_target(Format::RGBA16_FLOAT, "RDG MainCameraNormal"), "MainCameraNormal"));
                    parameters.position = pass_builder.write(pass_builder.createTransientTexture(create_color_target(Format::RGBA32_FLOAT, "RDG MainCameraPosition"), "MainCameraPosition"));
                    parameters.material = pass_builder.write(pass_builder.createTransientTexture(create_color_target(Format::RGBA8_UNORM, "RDG MainCameraMaterial"), "MainCameraMaterial"));
                    parameters.depth = pass_builder.write(pass_builder.createTransientTexture(depth_desc, "MainCameraDepth"));
                    parameters.primitive_scene_buffer = pass_builder.write(pass_builder.createTransientBuffer(primitive_scene_buffer_desc, "MainCameraPrimitiveSceneBuffer"));

                    pass_builder.blackboard().set<GBufferAlbedoKey>(parameters.albedo);
                    pass_builder.blackboard().set<GBufferNormalKey>(parameters.normal);
                    pass_builder.blackboard().set<GBufferPositionKey>(parameters.position);
                    pass_builder.blackboard().set<GBufferMaterialKey>(parameters.material);
                    pass_builder.blackboard().set<GBufferDepthKey>(parameters.depth);
                    pass_builder.blackboard().set<InstanceBufferKey>(parameters.primitive_scene_buffer);
                },
                [&gbuffer_mesh_processor](const GBufferPassParameters& parameters, const RenderGraphPassContext& context, RenderGraphCommandList& command_list) {
                    DO_ASSERT(context.getView() != nullptr, "GBufferPass view is null");
                    const auto& view_mesh_context = context.getView()->getMeshDrawContext();

                    const auto device = context.getGfxContext()->getDevice();
                    const auto albedo = command_list.resolveTexture(parameters.albedo);
                    const auto normal = command_list.resolveTexture(parameters.normal);
                    const auto position = command_list.resolveTexture(parameters.position);
                    const auto material = command_list.resolveTexture(parameters.material);
                    const auto depth = command_list.resolveTexture(parameters.depth);
                    const auto primitive_scene_buffer = command_list.resolveBuffer(parameters.primitive_scene_buffer);
                    auto framebuffer = device->createFramebuffer(
                        FramebufferDesc()
                            .addColorAttachment(albedo)
                            .addColorAttachment(normal)
                            .addColorAttachment(position)
                            .addColorAttachment(material)
                            .setDepthAttachment(depth)
                    );
                    const auto viewport_state = rendering_pipeline_utils::BuildViewportState(*context.getView(), context.getGfxContext()->getSwapchainExtent2d());

                    command_list.open();
                    command_list.beginMarker("GBufferPass");
                    command_list.setTextureState(parameters.albedo, AllSubresources, ResourceStates::RenderTarget);
                    command_list.setTextureState(parameters.normal, AllSubresources, ResourceStates::RenderTarget);
                    command_list.setTextureState(parameters.position, AllSubresources, ResourceStates::RenderTarget);
                    command_list.setTextureState(parameters.material, AllSubresources, ResourceStates::RenderTarget);
                    command_list.setTextureState(parameters.depth, AllSubresources, ResourceStates::DepthWrite);
                    command_list.commitBarriers();
                    command_list.clearTextureFloat(parameters.albedo, AllSubresources, Color(0.08f, 0.09f, 0.11f, 1.0f));
                    command_list.clearTextureFloat(parameters.normal, AllSubresources, Color(0.0f, 0.0f, 0.0f, 1.0f));
                    command_list.clearTextureFloat(parameters.position, AllSubresources, Color(0.0f, 0.0f, 0.0f, 1.0f));
                    command_list.clearTextureFloat(parameters.material, AllSubresources, Color(0.0f, 1.0f, 1.0f, 1.0f));
                    command_list.clearDepthStencilTexture(parameters.depth, AllSubresources, true, 1.0f, false, 0);
                    MeshDrawCommandDispatcher::uploadInstanceTransforms(view_mesh_context, parameters.primitive_scene_buffer, command_list);
                    MeshDrawCommandDispatcher::dispatch(
                        MeshPassType::GBuffer,
                        view_mesh_context,
                        view_mesh_context.getMeshPassCommands(MeshPassType::GBuffer),
                        framebuffer,
                        viewport_state,
                        GfxGraphicsPipelineHandle{},
                        primitive_scene_buffer,
                        gbuffer_mesh_processor.getConstantBuffer(),
                        command_list
                    );
                    command_list.setTextureState(parameters.albedo, AllSubresources, ResourceStates::ShaderResource);
                    command_list.setTextureState(parameters.normal, AllSubresources, ResourceStates::ShaderResource);
                    command_list.setTextureState(parameters.position, AllSubresources, ResourceStates::ShaderResource);
                    command_list.setTextureState(parameters.material, AllSubresources, ResourceStates::ShaderResource);
                    command_list.setTextureState(parameters.depth, AllSubresources, ResourceStates::ShaderResource);
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
                [pass_context](RenderGraphPassBuilder& pass_builder, ShadowPassParameters& parameters) {
                    const auto swapchain_extent = pass_context.gfx_context->getSwapchainExtent2d();
                    const auto* primitive_scene_buffer = pass_builder.blackboard().get<InstanceBufferKey, RenderGraphBufferHandle>();
                    DO_ASSERT(primitive_scene_buffer, "DirectionalShadowPass primitive scene buffer is missing");

                    RenderGraphTextureDesc shadow_desc{};
                    shadow_desc.desc = TextureDesc()
                        .setDimension(TextureDimension::Texture2D)
                        .setWidth(static_cast<UInt32>(swapchain_extent.x))
                        .setHeight(static_cast<UInt32>(swapchain_extent.y))
                        .setFormat(Format::D32)
                        .setIsRenderTarget(true)
                        .setIsShaderResource(true)
                        .enableAutomaticStateTracking(ResourceStates::DepthWrite)
                        .setDebugName("RDG ShadowMap");

                    parameters.shadow_map = pass_builder.write(pass_builder.createTransientTexture(shadow_desc, "ShadowMap"));
                    parameters.primitive_scene_buffer = pass_builder.read(*primitive_scene_buffer);
                    pass_builder.blackboard().set<ShadowMapKey>(parameters.shadow_map);
                },
                [&directional_shadow_mesh_processor](const ShadowPassParameters& parameters, const RenderGraphPassContext& context, RenderGraphCommandList& command_list) {
                    DO_ASSERT(context.getView() != nullptr, "DirectionalShadowPass view is null");
                    const auto& view_mesh_context = context.getView()->getMeshDrawContext();

                    const auto device = context.getGfxContext()->getDevice();
                    const auto shadow_map = command_list.resolveTexture(parameters.shadow_map);
                    const auto primitive_scene_buffer = command_list.resolveBuffer(parameters.primitive_scene_buffer);
                    auto framebuffer = device->createFramebuffer(FramebufferDesc().setDepthAttachment(shadow_map));
                    const auto viewport_state = rendering_pipeline_utils::BuildViewportState(*context.getView(), context.getGfxContext()->getSwapchainExtent2d());

                    command_list.open();
                    command_list.beginMarker("DirectionalShadowPass");
                    command_list.setTextureState(parameters.shadow_map, AllSubresources, ResourceStates::DepthWrite);
                    command_list.commitBarriers();
                    command_list.clearDepthStencilTexture(parameters.shadow_map, AllSubresources, true, 1.0f, false, 0);
                    MeshDrawCommandDispatcher::dispatch(
                        MeshPassType::DirectionalShadow,
                        view_mesh_context,
                        view_mesh_context.getMeshPassCommands(MeshPassType::DirectionalShadow),
                        framebuffer,
                        viewport_state,
                        GfxGraphicsPipelineHandle{},
                        primitive_scene_buffer,
                        directional_shadow_mesh_processor.getConstantBuffer(),
                        command_list
                    );
                    command_list.setTextureState(parameters.shadow_map, AllSubresources, ResourceStates::ShaderResource);
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
