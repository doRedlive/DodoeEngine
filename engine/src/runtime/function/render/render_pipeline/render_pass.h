// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/render/render_graph/render_graph_pass.h"

namespace dodoe {

    class RenderGraphPassBuilder;
    class RenderView;
    struct RenderPassContext;

    class IRenderPass {
    public:
        virtual ~IRenderPass() = default;

        [[nodiscard]] virtual const String& getName() const = 0;
        [[nodiscard]] virtual RenderGraphPassFlags getFlags() const = 0;

        virtual void setup(RenderGraphPassBuilder& builder,
                           const RenderPassContext& context,
                           const RenderView& view) = 0;

        virtual void execute(const RenderGraphPassContext& context,
                             DrawCommandList& cmd) = 0;
    };

} // namespace dodoe
