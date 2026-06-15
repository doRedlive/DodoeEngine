#pragma once

#include "dopch.h"

#include "../rendering_pass_context.h"

namespace dodoe {

    class RenderGraphBuilder;

    namespace RenderingPipelinePasses {

        void AddLightingGraphPasses(RenderGraphBuilder& graph, const RenderingPassContext& pass_context);

    } // namespace RenderingPipelinePasses

} // dodoe
