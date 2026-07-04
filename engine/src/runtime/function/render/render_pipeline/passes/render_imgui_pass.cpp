// do@Redlive

#include "render_pipeline_passes.h"

#include "render_pass_blackboard_keys.h"
#include "../render_pipeline_pass_utils.h"

#include "runtime/function/graphics/gfx_context.h"
#include "runtime/function/render/render_graph/render_graph_builder.h"
#include "runtime/function/render/render_pipeline/render_feature/imgui_render_resources.h"

#ifdef DODOE_DEBUG
#include "imgui/imgui.h"
#endif

namespace dodoe::RenderPipelinePass {

    void RenderImGuiPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context, ImGuiRenderResources& resources) {
#ifdef DODOE_DEBUG
        if (!ImGui::GetCurrentContext()) return;

        struct ImGuiPassParameters {
            RenderGraphTextureHandle output{};
        };

        graph.addPass<ImGuiPassParameters>(
            "ImGuiPass",
            RenderGraphPassFlags::Raster | RenderGraphPassFlags::NeverCull,
            [pass_context](RenderGraphPassBuilder& pass_builder, ImGuiPassParameters& parameters) {
                const auto swapchain_extent = pass_context.gfx_context->getSwapchainExtent2d();
                parameters.output = pass_builder.write(pass_builder.createTransientTexture(
                    rendering_pipeline_utils::MakeSwapchainRT2D(swapchain_extent, GfxFormat::RGBA8_UNORM, "RDG ImGuiColor"),
                    "ImGuiColor"));
                pass_builder.blackboard().set<ImGuiColorKey>(parameters.output);
            },
            [pass_context, &resources](const ImGuiPassParameters& parameters, const RenderGraphPassContext& context, DrawCommandList& command_list) {
                resources.renderImGui(pass_context, context, command_list, context.resolveTexture(parameters.output));
            }
        );
#else
        (void)graph;
        (void)pass_context;
        (void)resources;
#endif
    }

} // namespace dodoe::RenderPipelinePass
