// do@Redlive

#include "render_pipeline_passes.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "render_pass_blackboard_keys.h"

#include "../render_pipeline_pass_utils.h"
#include "../render_pass_context.h"

#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/framework/shader_parameter.h"
#include "runtime/function/render/framework/global_samplers.h"

namespace dodoe::RenderPipelinePass {

    struct SingleInputPassParameters {
        RenderGraphTextureHandle input{};
        RenderGraphTextureHandle output{};
    };

    BEGIN_SHADER_PARAMETER_STRUCT(SingleInputPassShaderParams)
        SHADER_PARAMETER(TextureSRV, 0, input)
        SHADER_PARAMETER(Sampler,    0, sampler)
    END_SHADER_PARAMETER_STRUCT(func(input); func(sampler);)

    struct ColorGradingPushConstants {
        Float exposure = 1.0f;
        Float contrast = 1.0f;
        Float saturation = 1.0f;
        Float padding = 0.0f;
    };

    BEGIN_SHADER_PARAMETER_STRUCT(ColorGradingPassShaderParams)
        SHADER_PARAMETER(TextureSRV, 0, input)
        SHADER_PARAMETER(Sampler,    0, sampler)
        ShaderParameter<ShaderParamType::PushConstants, 0, ColorGradingPushConstants> push_constants{};
    END_SHADER_PARAMETER_STRUCT(func(input); func(sampler); func(push_constants);)

    void RenderToneMappingPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context) {
        DO_ASSERT(pass_context.isValid(), "RenderingPipeline pass context is invalid");
        const auto& shader_library = *pass_context.getShaderLibrary();

        graph.addPass<SingleInputPassParameters>(
            "ToneMappingPass",
            RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
            [pass_context](RenderGraphPassBuilder& pass_builder, SingleInputPassParameters& parameters) {
                const auto swapchain_extent = pass_context.gfx_context->getSwapchainExtent2d();
                const auto* hdr = pass_builder.blackboard().get<SceneHdrKey, RenderGraphTextureHandle>();
                DO_ASSERT(hdr, "ToneMappingPass hdr input is missing");
                parameters.input = pass_builder.read(*hdr);
                parameters.output = pass_builder.write(pass_builder.createTransientTexture(
                    rendering_pipeline_utils::MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA8_UNORM, "RDG MainCameraToneMappedColor"),
                    "MainCameraToneMappedColor"));
                pass_builder.blackboard().set<ToneMappedColorKey>(parameters.output);
            },
            [pass_context, &shader_library](const SingleInputPassParameters& parameters, const RenderGraphPassContext& context, DrawCommandList& command_list) {
                const auto device = context.getGfxContext()->getDevice();
                const auto input_handle = context.resolveTexture(parameters.input);
                const auto output_handle = context.resolveTexture(parameters.output);

                auto framebuffer_desc = GfxFramebufferDesc().addColorAttachment(output_handle);
                auto framebuffer_ptr = create_ref<GfxFramebufferHandle>();
                command_list.createFramebuffer(device, framebuffer_desc, framebuffer_ptr.get());

                SingleInputPassShaderParams shader_params;
                shader_params.input.value = parameters.input;
                shader_params.sampler.value = GlobalSamplers::screen();

                const auto binding_layout = ShaderBindingReflector<SingleInputPassShaderParams>::getOrCreateLayout(device);

                auto binding_set_ptr = create_ref<GfxBindingSetHandle>();
                ShaderBindingReflector<SingleInputPassShaderParams>::createBindingSetDeferred(
                    command_list, device, binding_layout, shader_params, binding_set_ptr.get(),
                    [&](auto h) { return context.resolveTexture(h); },
                    [&](auto h) { return context.resolveBuffer(h); }
                );

                GfxFramebufferInfo framebuffer_info(framebuffer_desc);
                const auto pipeline = pass_context.getPipelineStateCache()->resolveGraphicsPipeline(
                    rendering_pipeline_utils::BuildFullscreenPipelineDesc(
                        shader_library.getFullscreenVertexShader(),
                        shader_library.getToneMappingPixelShader(),
                        binding_layout
                    ),
                    framebuffer_info
                );
                const auto viewport_state = rendering_pipeline_utils::BuildViewportState(*context.getView(), context.getGfxContext()->getSwapchainExtent2d());

                command_list.setTextureState(input_handle, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.setTextureState(output_handle, GfxAllSubresources, GfxResourceStates::RenderTarget);
                command_list.commitBarriers();
                command_list.clearTextureFloat(output_handle, GfxAllSubresources, GfxColor(0.0f, 0.0f, 0.0f, 1.0f));
                command_list.setGraphicsState(
                    GfxGraphicsState()
                        .setPipeline(pipeline)
                        .setFramebuffer(*framebuffer_ptr)
                        .setViewport(viewport_state)
                        .addBindingSet(*binding_set_ptr)
                );
                command_list.draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
                command_list.setTextureState(output_handle, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.commitBarriers();
            }
        );
    }

    void RenderColorGradingPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context) {
        DO_ASSERT(pass_context.isValid(), "RenderingPipeline pass context is invalid");
        const auto& shader_library = *pass_context.getShaderLibrary();

        graph.addPass<SingleInputPassParameters>(
            "ColorGradingPass",
            RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
            [pass_context](RenderGraphPassBuilder& pass_builder, SingleInputPassParameters& parameters) {
                const auto swapchain_extent = pass_context.gfx_context->getSwapchainExtent2d();
                const auto* input = pass_builder.blackboard().get<ToneMappedColorKey, RenderGraphTextureHandle>();
                DO_ASSERT(input, "ColorGradingPass input is missing");
                parameters.input = pass_builder.read(*input);
                parameters.output = pass_builder.write(pass_builder.createTransientTexture(
                    rendering_pipeline_utils::MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA8_UNORM, "RDG MainCameraColorGradedColor"),
                    "MainCameraColorGradedColor"));
                pass_builder.blackboard().set<SceneColorKey>(parameters.output);
            },
            [pass_context, &shader_library](const SingleInputPassParameters& parameters, const RenderGraphPassContext& context, DrawCommandList& command_list) {
                const auto device = context.getGfxContext()->getDevice();
                const auto input_handle = context.resolveTexture(parameters.input);
                const auto output_handle = context.resolveTexture(parameters.output);

                auto framebuffer_desc = GfxFramebufferDesc().addColorAttachment(output_handle);
                auto framebuffer_ptr = create_ref<GfxFramebufferHandle>();
                command_list.createFramebuffer(device, framebuffer_desc, framebuffer_ptr.get());

                ColorGradingPassShaderParams shader_params;
                shader_params.input.value = parameters.input;
                shader_params.sampler.value = GlobalSamplers::screen();
                shader_params.push_constants.value = ColorGradingPushConstants{};

                const auto binding_layout = ShaderBindingReflector<ColorGradingPassShaderParams>::getOrCreateLayout(device);

                auto binding_set_ptr = create_ref<GfxBindingSetHandle>();
                ShaderBindingReflector<ColorGradingPassShaderParams>::createBindingSetDeferred(
                    command_list, device, binding_layout, shader_params, binding_set_ptr.get(),
                    [&](auto h) { return context.resolveTexture(h); },
                    [&](auto h) { return context.resolveBuffer(h); }
                );

                GfxFramebufferInfo framebuffer_info(framebuffer_desc);
                const auto pipeline = pass_context.getPipelineStateCache()->resolveGraphicsPipeline(
                    rendering_pipeline_utils::BuildFullscreenPipelineDesc(
                        shader_library.getFullscreenVertexShader(),
                        shader_library.getColorGradingPixelShader(),
                        binding_layout
                    ),
                    framebuffer_info
                );
                const auto viewport_state = rendering_pipeline_utils::BuildViewportState(*context.getView(), context.getGfxContext()->getSwapchainExtent2d());

                command_list.setTextureState(input_handle, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.setTextureState(output_handle, GfxAllSubresources, GfxResourceStates::RenderTarget);
                command_list.commitBarriers();
                command_list.clearTextureFloat(output_handle, GfxAllSubresources, GfxColor(0.0f, 0.0f, 0.0f, 1.0f));
                command_list.setGraphicsState(
                    GfxGraphicsState()
                        .setPipeline(pipeline)
                        .setFramebuffer(*framebuffer_ptr)
                        .setViewport(viewport_state)
                        .addBindingSet(*binding_set_ptr)
                );
                command_list.draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
                command_list.setTextureState(output_handle, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.commitBarriers();
            }
        );
    }

    void RenderFxaaPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context) {
        DO_ASSERT(pass_context.isValid(), "RenderingPipeline pass context is invalid");
        const auto& shader_library = *pass_context.getShaderLibrary();

        graph.addPass<SingleInputPassParameters>(
            "FXAAPass",
            RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
            [pass_context](RenderGraphPassBuilder& pass_builder, SingleInputPassParameters& parameters) {
                const auto swapchain_extent = pass_context.gfx_context->getSwapchainExtent2d();
                const auto* input = pass_builder.blackboard().get<SceneColorKey, RenderGraphTextureHandle>();
                DO_ASSERT(input, "FXAAPass input is missing");
                parameters.input = pass_builder.read(*input);
                parameters.output = pass_builder.write(pass_builder.createTransientTexture(
                    rendering_pipeline_utils::MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA8_UNORM, "RDG MainCameraFxaaColor"),
                    "MainCameraFxaaColor"));
                pass_builder.blackboard().set<SceneColorKey>(parameters.output);
            },
            [pass_context, &shader_library](const SingleInputPassParameters& parameters, const RenderGraphPassContext& context, DrawCommandList& command_list) {
                const auto device = context.getGfxContext()->getDevice();
                const auto input_handle = context.resolveTexture(parameters.input);
                const auto output_handle = context.resolveTexture(parameters.output);

                auto framebuffer_desc = GfxFramebufferDesc().addColorAttachment(output_handle);
                auto framebuffer_ptr = create_ref<GfxFramebufferHandle>();
                command_list.createFramebuffer(device, framebuffer_desc, framebuffer_ptr.get());

                SingleInputPassShaderParams shader_params;
                shader_params.input.value = parameters.input;
                shader_params.sampler.value = GlobalSamplers::screen();

                const auto binding_layout = ShaderBindingReflector<SingleInputPassShaderParams>::getOrCreateLayout(device);

                auto binding_set_ptr = create_ref<GfxBindingSetHandle>();
                ShaderBindingReflector<SingleInputPassShaderParams>::createBindingSetDeferred(
                    command_list, device, binding_layout, shader_params, binding_set_ptr.get(),
                    [&](auto h) { return context.resolveTexture(h); },
                    [&](auto h) { return context.resolveBuffer(h); }
                );

                GfxFramebufferInfo framebuffer_info(framebuffer_desc);
                const auto pipeline = pass_context.getPipelineStateCache()->resolveGraphicsPipeline(
                    rendering_pipeline_utils::BuildFullscreenPipelineDesc(
                        shader_library.getFullscreenVertexShader(),
                        shader_library.getFxaaPixelShader(),
                        binding_layout
                    ),
                    framebuffer_info
                );
                const auto viewport_state = rendering_pipeline_utils::BuildViewportState(*context.getView(), context.getGfxContext()->getSwapchainExtent2d());

                command_list.setTextureState(input_handle, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.setTextureState(output_handle, GfxAllSubresources, GfxResourceStates::RenderTarget);
                command_list.commitBarriers();
                command_list.clearTextureFloat(output_handle, GfxAllSubresources, GfxColor(0.0f, 0.0f, 0.0f, 1.0f));
                command_list.setGraphicsState(
                    GfxGraphicsState()
                        .setPipeline(pipeline)
                        .setFramebuffer(*framebuffer_ptr)
                        .setViewport(viewport_state)
                        .addBindingSet(*binding_set_ptr)
                );
                command_list.draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
                command_list.setTextureState(output_handle, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.commitBarriers();
            }
        );
    }

} // namespace dodoe::RenderPipelinePass
