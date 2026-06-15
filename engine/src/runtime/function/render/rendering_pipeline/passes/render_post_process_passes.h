#pragma once

#include "dopch.h"

#include "../rendering_pass_context.h"

namespace dodoe {

    class RenderGraphBuilder;

    namespace RenderingPipelinePasses {

        void AddPostProcessGraphPasses(RenderGraphBuilder& graph, const RenderingPassContext& pass_context);
        void AddPresentGraphPass(RenderGraphBuilder& graph, const RenderingPassContext& pass_context);

    } // namespace RenderingPipelinePasses

} // dodoe
