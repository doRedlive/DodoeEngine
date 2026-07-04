// do@Redlive

#pragma once

#include "dopch.h"

#include "../render_pass_context.h"

namespace dodoe {

    class ImGuiRenderResources;
    class SpriteRenderResources;
    class RenderGraphBuilder;
    class RenderView;

    namespace RenderPipelinePass {

        void RenderGBufferPass(RenderGraphBuilder& graph, const RenderView& view, const RenderPassContext& pass_context);
        void RenderDirectionalShadowPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context);

        void RenderSkyboxPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context);
        void RenderDeferredLightPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context);

        void RenderSpritePass(RenderGraphBuilder& graph, const RenderView& view, const RenderPassContext& pass_context, SpriteRenderResources& resources);

        void RenderPostProcessPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context);
        void RenderToneMappingPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context);
        void RenderColorGradingPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context);
        void RenderFxaaPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context);
        void RenderImGuiPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context, ImGuiRenderResources& resources);

        void RenderPresentPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context);

    } // namespace RenderPipelinePass

} // dodoe
