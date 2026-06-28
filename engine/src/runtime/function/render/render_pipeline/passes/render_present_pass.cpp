// do@Redlive

#include "render_pipeline_passes.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "render_pass_blackboard_keys.h"

#include "../render_pipeline_pass_utils.h"

#include "runtime/function/render/framework/pipeline_state_cache.h"
#include "runtime/function/render/framework/shader_library.h"
#include "runtime/function/render/framework/shader_parameter.h"
#include "runtime/function/render/framework/global_samplers.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"

namespace dodoe::RenderPipelinePass {

    struct PresentPushConstants {
        float viewport_pos[2]{};
        float viewport_size[2]{};
    };

    BEGIN_SHADER_PARAMETER_STRUCT(PresentPassShaderParams)
        ShaderParameter<ShaderParamType::PushConstants, 0, PresentPushConstants> push_constants{};
        SHADER_PARAMETER(TextureSRV, 0, scene_color)
        SHADER_PARAMETER(TextureSRV, 1, imgui_color)
        SHADER_PARAMETER(Sampler,    0, sampler)
    END_SHADER_PARAMETER_STRUCT(func(push_constants); func(scene_color); func(imgui_color); func(sampler);)

    struct PresentPassParameters {
        RenderGraphTextureHandle scene_color{};
        RenderGraphTextureHandle imgui_color{};
        RenderGraphTextureHandle backbuffer{};
    };

    void RenderPresentPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context) {
        DO_ASSERT(pass_context.isValid(), "RenderingPipeline pass context is invalid");
        const auto& shader_library = *pass_context.getShaderLibrary();

        graph.addPass<PresentPassParameters>(
            "PresentPass",
            RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
            [](RenderGraphPassBuilder& pass_builder, PresentPassParameters& parameters) {
                const auto* scene_color = pass_builder.blackboard().get<FxaaColorKey, RenderGraphTextureHandle>();
                DO_ASSERT(scene_color, "PresentPass scene color is missing");
                parameters.scene_color = pass_builder.read(*scene_color);
                const auto* imgui_color = pass_builder.blackboard().get<ImGuiColorKey, RenderGraphTextureHandle>();
                parameters.imgui_color = imgui_color ? pass_builder.read(*imgui_color) : parameters.scene_color;
                parameters.backbuffer = pass_builder.write(pass_builder.importBackBuffer("PresentBackBuffer"));
            },
            [pass_context, &shader_library](const PresentPassParameters& parameters, const RenderGraphPassContext& context, DrawCommandList& command_list) {
                const auto device = context.getGfxContext()->getDevice();
                const auto scene_color_handle = context.resolveTexture(parameters.scene_color);
                const auto imgui_color_handle = context.resolveTexture(parameters.imgui_color);
                const auto backbuffer = context.resolveTexture(parameters.backbuffer);

                auto framebuffer_desc = GfxFramebufferDesc().addColorAttachment(backbuffer);
                auto framebuffer_ptr = create_ref<GfxFramebufferHandle>();
                command_list.createFramebuffer(device, framebuffer_desc, framebuffer_ptr.get());

                PresentPassShaderParams shader_params;
                shader_params.scene_color.value = parameters.scene_color;
                shader_params.imgui_color.value = parameters.imgui_color;
                shader_params.sampler.value = GlobalSamplers::screen();

                const auto viewport_rect = context.getView()->getViewportRect();
                const auto swapchain_extent = context.getGfxContext()->getSwapchainExtent2d();
                shader_params.push_constants.value.viewport_pos[0] = static_cast<float>(viewport_rect.x);
                shader_params.push_constants.value.viewport_pos[1] = static_cast<float>(viewport_rect.y);
                shader_params.push_constants.value.viewport_size[0] = viewport_rect.z > 0 ? static_cast<float>(viewport_rect.z) : static_cast<float>(swapchain_extent.x);
                shader_params.push_constants.value.viewport_size[1] = viewport_rect.w > 0 ? static_cast<float>(viewport_rect.w) : static_cast<float>(swapchain_extent.y);

                const auto binding_layout = ShaderBindingReflector<PresentPassShaderParams>::getOrCreateLayout(device);

                auto binding_set_ptr = create_ref<GfxBindingSetHandle>();
                ShaderBindingReflector<PresentPassShaderParams>::createBindingSetDeferred(
                    command_list, device, binding_layout, shader_params, binding_set_ptr.get(),
                    [&](auto h) { return context.resolveTexture(h); },
                    [&](auto h) { return context.resolveBuffer(h); }
                );

                GfxFramebufferInfo framebuffer_info(framebuffer_desc);
                const auto pipeline = pass_context.getPipelineStateCache()->resolveGraphicsPipeline(
                    rendering_pipeline_utils::BuildFullscreenPipelineDesc(
                        shader_library.getFullscreenVertexShader(),
                        shader_library.getPresentPixelShader(),
                        binding_layout
                    ),
                    framebuffer_info
                );
                const auto viewport_state = rendering_pipeline_utils::BuildViewportState(*context.getView(), swapchain_extent);

                command_list.setTextureState(scene_color_handle, GfxAllSubresources, GfxResourceStates::ShaderResource);
                command_list.setTextureState(backbuffer, GfxAllSubresources, GfxResourceStates::RenderTarget);
                command_list.commitBarriers();
                command_list.setGraphicsState(
                    GfxGraphicsState()
                        .setPipeline(pipeline)
                        .setFramebuffer(*framebuffer_ptr)
                        .setViewport(viewport_state)
                        .addBindingSet(*binding_set_ptr)
                );
                command_list.setPushConstants(&shader_params.push_constants.value, sizeof(PresentPushConstants));
                command_list.draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
                command_list.setTextureState(backbuffer, GfxAllSubresources, GfxResourceStates::Present);
                command_list.commitBarriers();
            }
        );
    }

} // namespace dodoe::RenderPipelinePass
