//
// Created by Redlive on 2026/3/25.
//

#ifndef DODOE_RENDER2D_GRAPH_H
#define DODOE_RENDER2D_GRAPH_H

#include "../render_graph.h"

#include "runtime/function/render/render_resource.h"

namespace dodoe {

    class Render2dGraph : public RenderGraph {
    public:
        void initialize(const RenderGraphCreateInfo& create_info) override;
        void shutdown() override;

        void prepare() override;
        void flush() override;

    private:
        Scope<RenderStage> sprite_stage_{nullptr};
        Scope<RenderStage> debug_stage_{nullptr};
    };

} // dodoe

#endif//DODOE_RENDER2D_GRAPH_H
