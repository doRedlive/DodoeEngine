// do@Redlive

#include "render_present_pass.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "render_pass_blackboard_keys.h"

#include "../render_pipeline_pass_utils.h"

#include "runtime/function/render/pipeline/pipeline_state_cache.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/function/render/shader/global_samplers.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_pipeline/render_graph_import_keys.h"
#include "runtime/function/render/render_pipeline/render_graph_import_registry.h"

namespace dodoe {

    struct PresentViewportCB {
        float viewport_pos[2]{};
        float viewport_size[2]{};
    };

    BEGIN_SHADER_PARAMETER_STRUCT(PresentPassShaderParams)
        ShaderParameter<ShaderParamType::PushConstants, 0, PresentViewportCB> viewport{};
        SHADER_PARAMETER(TextureSRV, 0, scene_color)
        SHADER_PARAMETER(TextureSRV, 1, imgui_color)
        SHADER_PARAMETER(Sampler,    0, sampler)
    END_SHADER_PARAMETER_STRUCT(func(viewport); func(scene_color); func(imgui_color); func(sampler);)

    struct PresentPassParameters {
        RenderGraphTextureHandle scene_color{};
        RenderGraphTextureHandle imgui_color{};
        RenderGraphTextureHandle backbuffer{};
    };

    void PresentPass::build(RenderGraphBuilder& graph,
                             const RenderPassBuildContext& context) {
        graph.addPass<PresentPassParameters>(
            "PresentPass",
            RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
            [&context](RenderGraphPassBuilder& pass_builder, PresentPassParameters& parameters) {
                const auto* scene_color = pass_builder.blackboard().get<SceneColorKey>();
                DO_ASSERT(scene_color, "PresentPass scene color is missing");
                parameters.scene_color = pass_builder.read(*scene_color);
                const auto* imgui_color = pass_builder.blackboard().get<ImGuiColorKey>();
                parameters.imgui_color = imgui_color ? pass_builder.read(*imgui_color) : parameters.scene_color;
                parameters.backbuffer = pass_builder.writeColor(pass_builder.importBackBuffer("PresentBackBuffer"));
            },
            [this](const PresentPassParameters& parameters, const RenderGraphPassContext& ctx, DrawCommandList& command_list) {
                const auto viewport_rect = ctx.getView()->getViewportRect();
                const auto swapchain_extent = ctx.getGfxContext()->getSwapchainExtent2d();

                PresentViewportCB viewport_data;
                viewport_data.viewport_pos[0] = static_cast<float>(viewport_rect.x);
                viewport_data.viewport_pos[1] = static_cast<float>(viewport_rect.y);
                viewport_data.viewport_size[0] = viewport_rect.z > 0 ? static_cast<float>(viewport_rect.z) : static_cast<float>(swapchain_extent.x);
                viewport_data.viewport_size[1] = viewport_rect.w > 0 ? static_cast<float>(viewport_rect.w) : static_cast<float>(swapchain_extent.y);
                PresentPassShaderParams shader_params;
                shader_params.viewport.value = viewport_data;
                shader_params.scene_color.value = parameters.scene_color;
                shader_params.imgui_color.value = parameters.imgui_color;
                shader_params.sampler.value = GlobalSamplers::screen();

                const auto binding_layout = ShaderBindingReflector<PresentPassShaderParams>::getOrCreateLayout();

                auto bs = ShaderBindingReflector<PresentPassShaderParams>::createBindingSetDeferred(
                    command_list, binding_layout, shader_params,
                    [&](auto h) { return ctx.resolveTexture(h); },
                    [&](auto h) { return ctx.resolveBuffer(h); }
                );

                if (!bs) {
                    DO_ERROR("PresentPass: Failed to create binding set");
                    return;
                }

                auto pipeline = ctx.getPipelineStateCache()->resolveGraphicsPipeline(
                    rendering_pipeline_utils::BuildFullscreenPipelineDesc(
                        ctx.getShaderLibrary()->getFullscreenVertexShader(),
                        ctx.getShaderLibrary()->getPresentPixelShader(),
                        binding_layout
                    ),
                    ctx.getRenderTargetSignature(),
                    command_list
                );
                const auto viewport_state = rendering_pipeline_utils::BuildViewportState(*ctx.getView(), swapchain_extent);

                DynamicArray<GfxBindingSetHandle> bs_arr = {bs};
                command_list.setGraphicsState(ctx.getFramebuffer(), pipeline, bs_arr, viewport_state);
                command_list.setPushConstants(viewport_data);
                command_list.draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
            }
        );
    }

} // namespace dodoe
