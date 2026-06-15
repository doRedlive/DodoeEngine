// do@Redlive
#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "render_lighting_passes.h"

#include "render_pass_blackboard_keys.h"

#include "../fullscreen_pass_shared_state.h"
#include "../rendering_pipeline_pass_utils.h"
#include "runtime/function/render/render_scene/render_scene.h"

#include "runtime/core/math/math.h"
#include "runtime/function/render/framework/pipeline_state_cache.h"
#include "runtime/function/render/framework/shader_library.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"

namespace dodoe::RenderingPipelinePasses {
    namespace {

        struct SkyboxPushConstants {
            Matrix4f inv_view_projection{1.0f};
        };

        struct DeferredLightPassConstants {
            Vector4f light_color_intensity{1.0f, 1.0f, 1.0f, 1.0f};
            Vector4f light_position_radius{0.0f, 0.0f, 0.0f, 0.0f};
            Vector4f light_direction_type{0.0f, 0.0f, 0.0f, 0.0f};
            Matrix4f light_view_projection{1.0f};
            Vector4f shadow_params{0.0025f, 0.65f, 0.0f, 0.0f};
            Vector4f camera_position{0.0f, 0.0f, 0.0f, 0.0f};
        };

        struct SkyboxPassParameters {
            RenderGraphTextureHandle depth{};
            RenderGraphTextureHandle hdr_color{};
        };

        struct DeferredLightPassParameters {
            RenderGraphTextureHandle albedo{};
            RenderGraphTextureHandle normal{};
            RenderGraphTextureHandle position{};
            RenderGraphTextureHandle material{};
            RenderGraphTextureHandle shadow_map{};
            RenderGraphTextureHandle hdr_color{};
            RenderGraphBufferHandle constant_buffer{};
        };

        void addSkyboxPass(RenderGraphBuilder& graph, const RenderingPassContext& pass_context) {
            DO_ASSERT(pass_context.isValid(), "RenderingPipeline pass context is invalid");
            const auto& shader_library = *pass_context.shader_library;

            graph.addPass<SkyboxPassParameters>(
                "SkyboxPass",
                RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
                [pass_context](RenderGraphPassBuilder& pass_builder, SkyboxPassParameters& parameters) {
                    const auto swapchain_extent = pass_context.gfx_context->getSwapchainExtent2d();
                    const auto* depth = pass_builder.blackboard().get<GBufferDepthKey, RenderGraphTextureHandle>();
                    DO_ASSERT(depth, "SkyboxPass depth is missing");

                    RenderGraphTextureDesc hdr_desc{};
                    hdr_desc.desc = GfxTextureDesc()
                        .setDimension(GfxTextureDimension::Texture2D)
                        .setWidth(static_cast<UInt32>(swapchain_extent.x))
                        .setHeight(static_cast<UInt32>(swapchain_extent.y))
                        .setFormat(GfxFormat::RGBA16_FLOAT)
                        .setIsRenderTarget(true)
                        .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
                        .setDebugName("RDG MainCameraHdrColor");

                    parameters.depth = pass_builder.read(*depth);
                    parameters.hdr_color = pass_builder.write(pass_builder.createTransientTexture(hdr_desc, "MainCameraHdrColor"));
                    pass_builder.blackboard().set<SceneHdrKey>(parameters.hdr_color);
                },
                [pass_context, &shader_library](const SkyboxPassParameters& parameters, const RenderGraphPassContext& context, RenderGraphCommandList& command_list) {
                    const auto device = context.getGfxContext()->getDevice();
                    const auto depth = command_list.resolveTexture(parameters.depth);
                    const auto hdr = command_list.resolveTexture(parameters.hdr_color);
                    auto framebuffer = device->createFramebuffer(GfxFramebufferDesc().addColorAttachment(hdr));
                    const auto* sky_light = context.getScene()->findSkyLight();
                    const auto skybox_texture = sky_light && sky_light->getCubemap() ? sky_light->getCubemap()->getGpuHandle() : GfxTextureHandle{nullptr};
                    const auto& sampler = pass_context.fullscreen_pass_shared_state->getScreenSampler();
                    const auto& binding_layout = pass_context.fullscreen_pass_shared_state->getSkyboxBindingLayout();
                    auto binding_set = device->createBindingSet(
                        GfxBindingSetDesc()
                            .addItem(GfxBindingSetItem::PushConstants(0, sizeof(SkyboxPushConstants)))
                            .addItem(GfxBindingSetItem::Texture_SRV(0, skybox_texture ? skybox_texture : hdr))
                            .addItem(GfxBindingSetItem::Texture_SRV(1, depth))
                            .addItem(GfxBindingSetItem::Sampler(0, sampler)),
                        binding_layout
                    );
                    const auto pipeline = pass_context.pipeline_state_cache->resolveGraphicsPipeline(
                        rendering_pipeline_utils::BuildFullscreenPipelineDesc(
                            shader_library.getFullscreenVertexShader(),
                            shader_library.getSkyboxPixelShader(),
                            binding_layout
                        ),
                        framebuffer->getFramebufferInfo()
                    );
                    SkyboxPushConstants constants{};
                    constants.inv_view_projection = Math::Inverse(context.getView()->getViewProjectionMatrix());

                    command_list.open();
                    command_list.beginMarker("SkyboxPass");
                    command_list.setTextureState(parameters.depth, GfxAllSubresources, GfxResourceStates::ShaderResource);
                    command_list.setTextureState(parameters.hdr_color, GfxAllSubresources, GfxResourceStates::RenderTarget);
                    command_list.commitBarriers();
                    command_list.clearTextureFloat(parameters.hdr_color, GfxAllSubresources, GfxColor(0.0f, 0.0f, 0.0f, 1.0f));
                    command_list.setGraphicsState(
                        GfxGraphicsState()
                            .setPipeline(pipeline)
                            .setFramebuffer(framebuffer)
                            .setViewport(rendering_pipeline_utils::BuildViewportState(*context.getView(), context.getGfxContext()->getSwapchainExtent2d()))
                            .addBindingSet(binding_set)
                    );
                    command_list.setPushConstants(&constants, sizeof(constants));
                    command_list.draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
                    command_list.endMarker();
                    command_list.close();
                }
            );
        }

        void addDeferredLightPass(RenderGraphBuilder& graph, const RenderingPassContext& pass_context) {
            DO_ASSERT(pass_context.isValid(), "RenderingPipeline pass context is invalid");
            const auto& shader_library = *pass_context.shader_library;

            graph.addPass<DeferredLightPassParameters>(
                "DeferredLightPass",
                RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
                [pass_context](RenderGraphPassBuilder& pass_builder, DeferredLightPassParameters& parameters) {
                    const auto* albedo = pass_builder.blackboard().get<GBufferAlbedoKey, RenderGraphTextureHandle>();
                    const auto* normal = pass_builder.blackboard().get<GBufferNormalKey, RenderGraphTextureHandle>();
                    const auto* position = pass_builder.blackboard().get<GBufferPositionKey, RenderGraphTextureHandle>();
                    const auto* material = pass_builder.blackboard().get<GBufferMaterialKey, RenderGraphTextureHandle>();
                    const auto* shadow = pass_builder.blackboard().get<ShadowMapKey, RenderGraphTextureHandle>();
                    const auto* hdr = pass_builder.blackboard().get<SceneHdrKey, RenderGraphTextureHandle>();
                    DO_ASSERT(albedo && normal && position && material && shadow && hdr, "DeferredLightPass blackboard resources are missing");
                    parameters.albedo = pass_builder.read(*albedo);
                    parameters.normal = pass_builder.read(*normal);
                    parameters.position = pass_builder.read(*position);
                    parameters.material = pass_builder.read(*material);
                    parameters.shadow_map = pass_builder.read(*shadow);
                    parameters.hdr_color = pass_builder.write(*hdr);
                    parameters.constant_buffer = pass_builder.importBuffer(pass_context.fullscreen_pass_shared_state->getDeferredLightConstantBuffer(), "DeferredLightConstantBuffer");
                },
                [pass_context, &shader_library](const DeferredLightPassParameters& parameters, const RenderGraphPassContext& context, RenderGraphCommandList& command_list) {
                    const auto device = context.getGfxContext()->getDevice();
                    const auto albedo = command_list.resolveTexture(parameters.albedo);
                    const auto normal = command_list.resolveTexture(parameters.normal);
                    const auto position = command_list.resolveTexture(parameters.position);
                    const auto material = command_list.resolveTexture(parameters.material);
                    const auto shadow = command_list.resolveTexture(parameters.shadow_map);
                    const auto hdr = command_list.resolveTexture(parameters.hdr_color);
                    const auto skybox = GfxTextureHandle{} ? GfxTextureHandle{} : hdr;
                    auto framebuffer = device->createFramebuffer(GfxFramebufferDesc().addColorAttachment(hdr));
                    const auto& sampler = pass_context.fullscreen_pass_shared_state->getScreenSampler();
                    const auto& binding_layout = pass_context.fullscreen_pass_shared_state->getDeferredLightBindingLayout();
                    auto binding_set = device->createBindingSet(
                        GfxBindingSetDesc()
                            .addItem(GfxBindingSetItem::ConstantBuffer(0, command_list.resolveBuffer(parameters.constant_buffer)))
                            .addItem(GfxBindingSetItem::Sampler(0, sampler))
                            .addItem(GfxBindingSetItem::Texture_SRV(0, albedo))
                            .addItem(GfxBindingSetItem::Texture_SRV(1, normal))
                            .addItem(GfxBindingSetItem::Texture_SRV(2, position))
                            .addItem(GfxBindingSetItem::Texture_SRV(3, shadow))
                            .addItem(GfxBindingSetItem::Texture_SRV(4, material))
                            .addItem(GfxBindingSetItem::Texture_SRV(5, skybox)),
                        binding_layout
                    );
                    const auto pipeline = pass_context.pipeline_state_cache->resolveGraphicsPipeline(
                        rendering_pipeline_utils::BuildFullscreenPipelineDesc(
                            shader_library.getFullscreenVertexShader(),
                            shader_library.getDeferredLightPixelShader(),
                            binding_layout
                        ),
                        framebuffer->getFramebufferInfo()
                    );
                    const auto viewport_state = rendering_pipeline_utils::BuildViewportState(*context.getView(), context.getGfxContext()->getSwapchainExtent2d());
                    const auto camera_position = rendering_pipeline_utils::ExtractCameraPosition(*context.getView());

                    command_list.open();
                    command_list.beginMarker("DeferredLightPass");
                    command_list.setTextureState(parameters.albedo, GfxAllSubresources, GfxResourceStates::ShaderResource);
                    command_list.setTextureState(parameters.normal, GfxAllSubresources, GfxResourceStates::ShaderResource);
                    command_list.setTextureState(parameters.position, GfxAllSubresources, GfxResourceStates::ShaderResource);
                    command_list.setTextureState(parameters.material, GfxAllSubresources, GfxResourceStates::ShaderResource);
                    command_list.setTextureState(parameters.shadow_map, GfxAllSubresources, GfxResourceStates::ShaderResource);
                    command_list.setTextureState(parameters.hdr_color, GfxAllSubresources, GfxResourceStates::RenderTarget);
                    command_list.commitBarriers();

                    const auto& directional_lights = context.getScene()->getDirectionalLights();
                    for (const auto& light : directional_lights) {
                        DeferredLightPassConstants constants{};
                        constants.light_color_intensity = Vector4f(light.color, light.irradiance);
                        constants.light_direction_type = Vector4f(glm::normalize(light.direction), 0.0f);
                        constants.light_view_projection = rendering_pipeline_utils::BuildDirectionalLightViewProjection(light.direction);
                        constants.shadow_params = Vector4f(0.005f, 0.2f, 0.005f, 2.0f);
                        constants.camera_position = Vector4f(camera_position, 0.0f);
                        command_list.writeBuffer(parameters.constant_buffer, &constants, sizeof(constants));
                        command_list.setGraphicsState(
                            GfxGraphicsState()
                                .setPipeline(pipeline)
                                .setFramebuffer(framebuffer)
                                .setViewport(viewport_state)
                                .addBindingSet(binding_set)
                        );
                        command_list.draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
                    }

                    const auto& point_lights = context.getScene()->getPointLights();
                    for (const auto& light : point_lights) {
                        DeferredLightPassConstants constants{};
                        constants.light_color_intensity = Vector4f(light.color, light.intensity);
                        constants.light_position_radius = Vector4f(light.position, light.radius);
                        constants.light_direction_type = Vector4f(0.0f, 0.0f, 0.0f, light.range);
                        constants.camera_position = Vector4f(camera_position, 0.0f);
                        command_list.writeBuffer(parameters.constant_buffer, &constants, sizeof(constants));
                        command_list.setGraphicsState(
                            GfxGraphicsState()
                                .setPipeline(pipeline)
                                .setFramebuffer(framebuffer)
                                .setViewport(viewport_state)
                                .addBindingSet(binding_set)
                        );
                        command_list.draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
                    }

                    command_list.setTextureState(parameters.hdr_color, GfxAllSubresources, GfxResourceStates::ShaderResource);
                    command_list.commitBarriers();
                    command_list.endMarker();
                    command_list.close();
                }
            );
        }
    } // namespace

    void AddLightingGraphPasses(RenderGraphBuilder& graph, const RenderingPassContext& pass_context) {
        addSkyboxPass(graph, pass_context);
        addDeferredLightPass(graph, pass_context);
    }
} // namespace dodoe::RenderingPipelinePasses
