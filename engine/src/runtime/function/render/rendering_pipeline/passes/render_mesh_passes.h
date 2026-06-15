#pragma once

#include "dopch.h"

#include "../rendering_pass_context.h"

namespace dodoe {

    class RenderGraphBuilder;
    class RenderView;

    namespace RenderingPipelinePasses {

        void AddMeshGraphPasses(RenderGraphBuilder& graph, const RenderView& view, const RenderingPassContext& pass_context);

    } // namespace RenderingPipelinePasses

} // dodoe
