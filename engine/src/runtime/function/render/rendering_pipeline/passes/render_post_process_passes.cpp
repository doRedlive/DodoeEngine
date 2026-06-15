// do@Redlive
#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "render_post_process_passes.h"

#include "render_pass_blackboard_keys.h"

#include "../fullscreen_pass_shared_state.h"
#include "../rendering_pipeline_pass_utils.h"

#include "runtime/function/render/framework/pipeline_state_cache.h"
#include "runtime/function/render/framework/shader_library.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"

namespace dodoe::RenderingPipelinePasses {
    namespace {

        struct ColorGradingConstants {
            Vector4f params{1.0f, 1.0f, 1.0f, 2.2f};
        };

        struct SingleInputFullscreenParameters {
            RenderGraphTextureHandle input{};
            RenderGraphTextureHandle output{};
        };

        struct PresentParameters {
            RenderGraphTextureHandle scene_color{};
            RenderGraphTextureHandle backbuffer{};
        };

        void addToneMappingPass(RenderGraphBuilder& graph, const RenderingPassContext& pass_context) {
            DO_ASSERT(pass_context.isValid(), "RenderingPipeline pass context is invalid");
            const auto& shader_library = *pass_context.shader_library;

            graph.addPass<SingleInputFullscreenParameters>(
                "ToneMappingPass",
                RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
                [pass_context](RenderGraphPassBuilder& pass_builder, SingleInputFullscreenParameters& parameters) {
                    const auto swapchain_extent = pass_context.gfx_context->getSwapchainExtent2d();
                    const auto* hdr = pass_builder.blackboard().get<SceneHdrKey, RenderGraphTextureHandle>();
                    DO_ASSERT(hdr, "ToneMappingPass hdr input is missing");
                    RenderGraphTextureDesc output_desc{};
                    output_desc.desc = GfxTextureDesc()
                        .setDimension(GfxTextureDimension::Texture2D)
                        .setWidth(static_cast<UInt32>(swapchain_extent.x))
                        .setHeight(static_cast<UInt32>(swapchain_extent.y))
                        .setFormat(GfxFormat::RGBA8_UNORM)
                        .setIsRenderTarget(true)
                        .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
                        .setDebugName("RDG MainCameraToneMappedColor");
                    parameters.input = pass_builder.read(*hdr);
                    parameters.output = pass_builder.write(pass_builder.createTransientTexture(output_desc, "MainCameraToneMappedColor"));
                    pass_builder.blackboard().set<ToneMappedColorKey>(parameters.output);
                },
                [pass_context, &shader_library](const SingleInputFullscreenParameters& parameters, const RenderGraphPassContext& context, RenderGraphCommandList& command_list) {
                    rendering_pipeline_utils::RecordSingleInputFullscreenPass(
                        context,
                        command_list,
                        pass_context.pipeline_state_cache->resolveGraphicsPipeline(
                            rendering_pipeline_utils::BuildFullscreenPipelineDesc(
                                shader_library.getFullscreenVertexShader(),
                                shader_library.getToneMappingPixelShader(),
                                pass_context.fullscreen_pass_shared_state->getSingleInputBindingLayout()
                            ),
                            GfxFramebufferInfo().addColorFormat(GfxFormat::RGBA8_UNORM)
                        ),
                        pass_context.fullscreen_pass_shared_state->getSingleInputBindingLayout(),
                        pass_context.fullscreen_pass_shared_state->getScreenSampler(),
                        parameters.input,
                        parameters.output,
                        "ToneMappingPass"
                    );
                }
            );
        }

        void addColorGradingPass(RenderGraphBuilder& graph, const RenderingPassContext& pass_context) {
            DO_ASSERT(pass_context.isValid(), "RenderingPipeline pass context is invalid");
            const auto& shader_library = *pass_context.shader_library;

            graph.addPass<SingleInputFullscreenParameters>(
                "ColorGradingPass",
                RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
                [pass_context](RenderGraphPassBuilder& pass_builder, SingleInputFullscreenParameters& parameters) {
                    const auto swapchain_extent = pass_context.gfx_context->getSwapchainExtent2d();
                    const auto* input = pass_builder.blackboard().get<ToneMappedColorKey, RenderGraphTextureHandle>();
                    DO_ASSERT(input, "ColorGradingPass input is missing");
                    RenderGraphTextureDesc output_desc{};
                    output_desc.desc = GfxTextureDesc()
                        .setDimension(GfxTextureDimension::Texture2D)
                        .setWidth(static_cast<UInt32>(swapchain_extent.x))
                        .setHeight(static_cast<UInt32>(swapchain_extent.y))
                        .setFormat(GfxFormat::RGBA8_UNORM)
                        .setIsRenderTarget(true)
                        .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
                        .setDebugName("RDG MainCameraColor");
                    parameters.input = pass_builder.read(*input);
                    parameters.output = pass_builder.write(pass_builder.createTransientTexture(output_desc, "MainCameraColor"));
                    pass_builder.blackboard().set<SceneColorKey>(parameters.output);
                },
                [pass_context, &shader_library](const SingleInputFullscreenParameters& parameters, const RenderGraphPassContext& context, RenderGraphCommandList& command_list) {
                    const auto device = context.getGfxContext()->getDevice();
                    const auto input_texture = command_list.resolveTexture(parameters.input);
                    const auto output_texture = command_list.resolveTexture(parameters.output);
                    auto framebuffer = device->createFramebuffer(GfxFramebufferDesc().addColorAttachment(output_texture));
                    const auto& sampler = pass_context.fullscreen_pass_shared_state->getScreenSampler();
                    const auto& binding_layout = pass_context.fullscreen_pass_shared_state->getColorGradingBindingLayout();
                    auto binding_set = device->createBindingSet(
                        GfxBindingSetDesc()
                            .addItem(GfxBindingSetItem::PushConstants(0, sizeof(ColorGradingConstants)))
                            .addItem(GfxBindingSetItem::Texture_SRV(0, input_texture))
                            .addItem(GfxBindingSetItem::Sampler(0, sampler)),
                        binding_layout
                    );
                    const auto pipeline = pass_context.pipeline_state_cache->resolveGraphicsPipeline(
                        rendering_pipeline_utils::BuildFullscreenPipelineDesc(
                            shader_library.getFullscreenVertexShader(),
                            shader_library.getColorGradingPixelShader(),
                            binding_layout
                        ),
                        framebuffer->getFramebufferInfo()
                    );
                    ColorGradingConstants constants{};

                    command_list.open();
                    command_list.beginMarker("ColorGradingPass");
                    command_list.setTextureState(parameters.input, GfxAllSubresources, GfxResourceStates::ShaderResource);
                    command_list.setTextureState(parameters.output, GfxAllSubresources, GfxResourceStates::RenderTarget);
                    command_list.commitBarriers();
                    command_list.clearTextureFloat(parameters.output, GfxAllSubresources, GfxColor(0.0f, 0.0f, 0.0f, 1.0f));
                    command_list.setGraphicsState(
                        GfxGraphicsState()
                            .setPipeline(pipeline)
                            .setFramebuffer(framebuffer)
                            .setViewport(rendering_pipeline_utils::BuildViewportState(*context.getView(), context.getGfxContext()->getSwapchainExtent2d()))
                            .addBindingSet(binding_set)
                    );
                    command_list.setPushConstants(&constants, sizeof(constants));
                    command_list.draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
                    command_list.setTextureState(parameters.output, GfxAllSubresources, GfxResourceStates::ShaderResource);
                    command_list.commitBarriers();
                    command_list.endMarker();
                    command_list.close();
                }
            );
        }

        void addFxaaPass(RenderGraphBuilder& graph, const RenderingPassContext& pass_context) {
            DO_ASSERT(pass_context.isValid(), "RenderingPipeline pass context is invalid");
            const auto& shader_library = *pass_context.shader_library;

            graph.addPass<SingleInputFullscreenParameters>(
                "FXAAPass",
                RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
                [pass_context](RenderGraphPassBuilder& pass_builder, SingleInputFullscreenParameters& parameters) {
                    const auto swapchain_extent = pass_context.gfx_context->getSwapchainExtent2d();
                    const auto* input = pass_builder.blackboard().get<SceneColorKey, RenderGraphTextureHandle>();
                    DO_ASSERT(input, "FXAAPass input is missing");
                    RenderGraphTextureDesc output_desc{};
                    output_desc.desc = GfxTextureDesc()
                        .setDimension(GfxTextureDimension::Texture2D)
                        .setWidth(static_cast<UInt32>(swapchain_extent.x))
                        .setHeight(static_cast<UInt32>(swapchain_extent.y))
                        .setFormat(GfxFormat::RGBA8_UNORM)
                        .setIsRenderTarget(true)
                        .enableAutomaticStateTracking(GfxResourceStates::ShaderResource)
                        .setDebugName("RDG MainCameraFxaaColor");
                    parameters.input = pass_builder.read(*input);
                    parameters.output = pass_builder.write(pass_builder.createTransientTexture(output_desc, "MainCameraFxaaColor"));
                    pass_builder.blackboard().set<FxaaColorKey>(parameters.output);
                },
                [pass_context, &shader_library](const SingleInputFullscreenParameters& parameters, const RenderGraphPassContext& context, RenderGraphCommandList& command_list) {
                    rendering_pipeline_utils::RecordSingleInputFullscreenPass(
                        context,
                        command_list,
                        pass_context.pipeline_state_cache->resolveGraphicsPipeline(
                            rendering_pipeline_utils::BuildFullscreenPipelineDesc(
                                shader_library.getFullscreenVertexShader(),
                                shader_library.getFxaaPixelShader(),
                                pass_context.fullscreen_pass_shared_state->getSingleInputBindingLayout()
                            ),
                            GfxFramebufferInfo().addColorFormat(GfxFormat::RGBA8_UNORM)
                        ),
                        pass_context.fullscreen_pass_shared_state->getSingleInputBindingLayout(),
                        pass_context.fullscreen_pass_shared_state->getScreenSampler(),
                        parameters.input,
                        parameters.output,
                        "FXAAPass"
                    );
                }
            );
        }

        void addPresentPass(RenderGraphBuilder& graph, const RenderingPassContext& pass_context) {
            DO_ASSERT(pass_context.isValid(), "RenderingPipeline pass context is invalid");
            const auto& shader_library = *pass_context.shader_library;

            graph.addPass<PresentParameters>(
                "PresentPass",
                RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
                [](RenderGraphPassBuilder& pass_builder, PresentParameters& parameters) {
                    const auto* scene_color = pass_builder.blackboard().get<FxaaColorKey, RenderGraphTextureHandle>();
                    DO_ASSERT(scene_color, "PresentPass scene color is missing");
                    parameters.scene_color = pass_builder.read(*scene_color);
                    parameters.backbuffer = pass_builder.write(pass_builder.importBackBuffer("PresentBackBuffer"));
                },
                [pass_context, &shader_library](const PresentParameters& parameters, const RenderGraphPassContext& context, RenderGraphCommandList& command_list) {
                    const auto device = context.getGfxContext()->getDevice();
                    const auto scene_color = command_list.resolveTexture(parameters.scene_color);
                    const auto backbuffer = command_list.resolveTexture(parameters.backbuffer);
                    auto framebuffer = device->createFramebuffer(GfxFramebufferDesc().addColorAttachment(backbuffer));
                    const auto& sampler = pass_context.fullscreen_pass_shared_state->getScreenSampler();
                    const auto& binding_layout = pass_context.fullscreen_pass_shared_state->getPresentBindingLayout();
                    auto binding_set = device->createBindingSet(
                        GfxBindingSetDesc()
                            .addItem(GfxBindingSetItem::PushConstants(0, sizeof(float) * 4))
                            .addItem(GfxBindingSetItem::Texture_SRV(0, scene_color))
                            .addItem(GfxBindingSetItem::Texture_SRV(1, scene_color))
                            .addItem(GfxBindingSetItem::Sampler(0, sampler)),
                        binding_layout
                    );
                    const auto pipeline = pass_context.pipeline_state_cache->resolveGraphicsPipeline(
                        rendering_pipeline_utils::BuildFullscreenPipelineDesc(
                            shader_library.getFullscreenVertexShader(),
                            shader_library.getPresentPixelShader(),
                            binding_layout
                        ),
                        framebuffer->getFramebufferInfo()
                    );
                    struct PresentPushConstants {
                        float viewport_pos[2];
                        float viewport_size[2];
                    } constants{};
                    const auto viewport_rect = context.getView()->getViewportRect();
                    const auto swapchain_extent = context.getGfxContext()->getSwapchainExtent2d();
                    constants.viewport_pos[0] = static_cast<float>(viewport_rect.x);
                    constants.viewport_pos[1] = static_cast<float>(viewport_rect.y);
                    constants.viewport_size[0] = viewport_rect.z > 0 ? static_cast<float>(viewport_rect.z) : static_cast<float>(swapchain_extent.x);
                    constants.viewport_size[1] = viewport_rect.w > 0 ? static_cast<float>(viewport_rect.w) : static_cast<float>(swapchain_extent.y);

                    command_list.open();
                    command_list.beginMarker("PresentPass");
                    command_list.setTextureState(parameters.scene_color, GfxAllSubresources, GfxResourceStates::ShaderResource);
                    command_list.setTextureState(parameters.backbuffer, GfxAllSubresources, GfxResourceStates::RenderTarget);
                    command_list.commitBarriers();
                    command_list.setGraphicsState(
                        GfxGraphicsState()
                            .setPipeline(pipeline)
                            .setFramebuffer(framebuffer)
                            .setViewport(rendering_pipeline_utils::BuildViewportState(*context.getView(), swapchain_extent))
                            .addBindingSet(binding_set)
                    );
                    command_list.setPushConstants(&constants, sizeof(constants));
                    command_list.draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
                    command_list.setTextureState(parameters.backbuffer, GfxAllSubresources, GfxResourceStates::Present);
                    command_list.commitBarriers();
                    command_list.endMarker();
                    command_list.close();
                }
            );
        }
    } // namespace

    void AddPostProcessGraphPasses(RenderGraphBuilder& graph, const RenderingPassContext& pass_context) {
        addToneMappingPass(graph, pass_context);
        addColorGradingPass(graph, pass_context);
        addFxaaPass(graph, pass_context);
    }

    void AddPresentGraphPass(RenderGraphBuilder& graph, const RenderingPassContext& pass_context) {
        addPresentPass(graph, pass_context);
    }

} // namespace dodoe::RenderingPipelinePasses