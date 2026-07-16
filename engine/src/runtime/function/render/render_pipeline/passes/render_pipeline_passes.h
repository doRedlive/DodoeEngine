// do@Redlive

#pragma once

#include "dopch.h"

#include "../render_pass_context.h"

namespace dodoe {

    class ImGuiRenderResource;
    class SpriteRenderResource;
    class DeferredLightRenderResource;
    class RenderGraphBuilder;
    class RenderView;

#ifdef DODOE_EDITOR_ENABLED
    class GizmoRenderResource;
#endif

    namespace RenderPipelinePass {

        void RenderGBufferPass(RenderGraphBuilder& graph, const RenderView& view, const RenderPassContext& pass_context);
        void RenderDirectionalShadowPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context);

        void RenderSkyboxPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context);
        void RenderDeferredLightPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context, DeferredLightRenderResource& resources);

        void RenderSpritePass(RenderGraphBuilder& graph, const RenderView& view, const RenderPassContext& pass_context, SpriteRenderResource& resources);

        void RenderPostProcessPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context);
        void RenderPostProcess2DPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context);
        void RenderImGuiPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context, ImGuiRenderResource& resources);

        void RenderPresentPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context);

        void RenderTestPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context);

#ifdef DODOE_EDITOR_ENABLED
        void RenderGizmoPass(RenderGraphBuilder& graph, const RenderPassContext& pass_context, GizmoRenderResource& resources);
#endif

    } // namespace RenderPipelinePass

} // dodoe
