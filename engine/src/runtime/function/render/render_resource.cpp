//
// Created by Redlive on 2026/3/25.
//

#include "render_resource.h"

namespace dodoe {

    RenderResource* g_render_resource = new RenderResource();

    void RenderResource::submit(const QuadDrawContext& context) {
        texture_draw_contexts_.push_back(context);
    }

    void RenderResource::submit(const LineDrawContext& context) {
        line_draw_contexts_.push_back(context);
    }

    void RenderResource::submit(const TextDrawContext& context) {
        text_draw_contexts_.push_back(context);
    }

    std::vector<QuadDrawContext> RenderResource::gain_quad_draw_contexts() {
        std::vector<QuadDrawContext> out;
        out.swap(texture_draw_contexts_);
        line_draw_contexts_.clear();
        text_draw_contexts_.clear();
        return out;
    }

} // dodoe
