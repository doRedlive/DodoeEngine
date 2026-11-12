// do@Redlive

#include "render_present_pass.h"

#include "runtime/function/graphics/gfx.h"
#include "runtime/function/graphics/gfx_context.h"

#include "render_pass_blackboard_keys.h"

#include "../render_pipeline_pass_utils.h"

#include "runtime/function/render/pipeline_state/pipeline_state_cache.h"
#include "runtime/function/render/shader/shader_library.h"
#include "runtime/function/render/shader/shader_parameter.h"
#include "runtime/function/render/shader/global_samplers.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_settings.h"
#include "runtime/function/render/render_pipeline/render_graph_import_keys.h"
#include "runtime/function/render/render_pipeline/render_graph_import_registry.h"

namespace dodoe {

    struct PresentViewportCB {
        float viewport_pos[2]{};
        float viewport_size[2]{};
    };

    BEGIN_SHADER_PARAMETER_STRUCT(PresentPassShaderParams, Pass)
        SHADER_PARAMETER_RAWCB(0, viewport_cb)
        SHADER_PARAMETER(TextureSRV, 1, scene_color)
        SHADER_PARAMETER(TextureSRV, 2, imgui_color)
        SHADER_PARAMETER(Sampler, 9, sampler)
    END_SHADER_PARAMETER_STRUCT(func(viewport_cb); func(scene_color); func(imgui_color); func(sampler);)

    struct PresentPassParameters {
        RenderGraphBufferHandle viewport_cb{};
        RenderGraphTextureHandle scene_color{};
        RenderGraphTextureHandle imgui_color{};
        RenderGraphTextureHandle backbuffer{};
    };

    void PresentPass::build(RenderGraphBuilder& graph,
                             const RenderPassBuildContext& context) {
        const auto pipeline_type = RenderSettings::GetRenderingPipelineType();
        const Bool is_2d = pipeline_type == RenderingPipelineType::Only2D || pipeline_type == RenderingPipelineType::OnlyGUI;
        if (!graph.blackboard().get<ImGuiColorKey>()) {
            struct EmptyOverlayParameters {};
            graph.addPass<EmptyOverlayParameters>("EmptyOverlay", RenderGraphPassFlags::Raster,
                [&context, is_2d](RenderGraphPassBuilder& builder, EmptyOverlayParameters&) {
                    RenderGraphAttachmentInfo attachment{};
                    attachment.load_op = LoadOp::Clear;
                    attachment.clear_color = GfxColor(0.0f, 0.0f, 0.0f, 0.0f);
                    const auto extent = is_2d ? Vector2i(1, 1) : context.gfx_context->getSwapchainExtent2D();
                    const auto texture = builder.writeColor(builder.createTransientTexture(
                        MakeRenderTarget2D(extent.x, extent.y, GfxFormat::RGBA8_UNORM, "RDG EmptyOverlay"),
                        "EmptyOverlay"), attachment);
                    builder.blackboard().set<ImGuiColorKey>(texture);
                }, [](const EmptyOverlayParameters&, const RenderGraphPassContext&, DrawCommandList&) {});
        }
        const auto binding_layouts = ShaderBindingReflector<PresentPassShaderParams>::getOrCreateLayouts();
        graph.addPass<PresentPassParameters>(
            "PresentPass",
            RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
            [&context](RenderGraphPassBuilder& pass_builder, PresentPassParameters& parameters) {
                const auto* scene_color = pass_builder.blackboard().get<SceneColorKey>();
                DO_ASSERT(scene_color, "PresentPass scene color is missing");
                parameters.scene_color = pass_builder.read(*scene_color);
                const auto* imgui_color = pass_builder.blackboard().get<ImGuiColorKey>();
                parameters.imgui_color = pass_builder.read(*imgui_color);
                parameters.backbuffer = pass_builder.writeColor(pass_builder.importBackBuffer("PresentBackBuffer"));
                RenderGraphBufferDesc cb_desc{};
                cb_desc.desc.setByteSize(sizeof(PresentViewportCB)).setIsConstantBuffer(true)
                    .enableAutomaticStateTracking(GfxResourceStates::CopyDest).setDebugName("PresentViewportCB");
                parameters.viewport_cb = pass_builder.createTransientBuffer(cb_desc, "PresentViewportCB");
                const auto rect = context.view.getViewportRect();
                const auto extent = context.gfx_context->getSwapchainExtent2D();
                PresentViewportCB data{};
                data.viewport_pos[0] = static_cast<float>(rect.x);
                data.viewport_pos[1] = static_cast<float>(rect.y);
                data.viewport_size[0] = static_cast<float>(rect.z > 0 ? rect.z : extent.x);
                data.viewport_size[1] = static_cast<float>(rect.w > 0 ? rect.w : extent.y);
                pass_builder.uploadBuffer(parameters.viewport_cb, &data, sizeof(data));
                pass_builder.readBuffer(parameters.viewport_cb, RenderGraphBufferUsage::Constant);
            },
            [binding_layouts, is_2d](const PresentPassParameters& parameters, const RenderGraphPassContext& ctx, DrawCommandList& command_list) {
                const auto swapchain_extent = ctx.getGfxContext()->getSwapchainExtent2D();
                const auto viewport_cb = ctx.resolveBuffer(parameters.viewport_cb);
                PresentPassShaderParams shader_params;
                shader_params.viewport_cb.value = viewport_cb;
                shader_params.scene_color.value = parameters.scene_color;
                shader_params.imgui_color.value = parameters.imgui_color;
                shader_params.sampler.value = GlobalSamplers::Screen();


                auto binding_sets = ShaderBindingReflector<PresentPassShaderParams>::createBindingSets(
                    *ctx.getSharedRenderService()->getBindingSetCache(),
                    command_list,
                    binding_layouts, shader_params,
                    [&](auto h) { return ctx.resolveTexture(h); },
                    [&](auto h) { return ctx.resolveBuffer(h); }, BindingSetFrequency::Pass);

                if (binding_sets.empty()) {
                    DO_ERROR("PresentPass: Failed to create binding set");
                    return;
                }

                auto pipeline = ctx.getPipelineStateCache()->resolveGraphicsPipeline(
                    rendering_pipeline_utils::BuildFullscreenPipelineDesc(
                        ctx.getShaderLibrary()->getFullscreenVertexShader(),
                        is_2d ? ctx.getShaderLibrary()->getPresent2DPixelShader() : ctx.getShaderLibrary()->getPresentPixelShader(),
                        binding_layouts
                    ),
                    ctx.getRenderTargetSignature(),
                    command_list
                );
                if (!pipeline) {
                    DO_ERROR("PresentPass: Failed to create pipeline");
                    return;
                }
                const auto viewport_state = rendering_pipeline_utils::BuildViewportState(*ctx.getView(), swapchain_extent);

                command_list.setGraphicsState(ctx.getFramebuffer(), pipeline, binding_sets, viewport_state);
                command_list.draw(GfxDrawArguments().setVertexCount(6).setInstanceCount(1));
            }
        );
    }

} // namespace dodoe
