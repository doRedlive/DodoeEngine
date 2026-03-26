//
// Created by Redlive on 2026/3/25.
//

#ifndef DODOE_DRAW_CONTEXT_H
#define DODOE_DRAW_CONTEXT_H

#include "dopch.h"

#include "renderer.h"
#include "runtime/function/render/backend/texture.h"

namespace dodoe {

    struct QuadDrawContext {
        Ref<Texture> texture{nullptr};
        Vector4f dst_rect{0.0f};
        Vector4f uv_rect{0.0f};
        Vector3f rotation{0.0f};
        Vector4f color{1.0f, 1.0f, 1.0f, 1.0f}; 

        RenderStageType stage{};
    };

    struct LineDrawContext {
        Vector2f start{0.0f};
        Vector2f end{0.0f};
        Vector4f color{1.0f, 1.0f, 1.0f, 1.0f};
        float thickness{2.0f};

        RenderStageType stage{};
    };

    struct TextDrawContext {
        Ref<Texture> texture{nullptr};
        Vector4f dst_rect{0.0f};
        Vector4f uv_rect{0.0f};
        Vector3f rotation{0.0f};
        Vector4f color{1.0f, 1.0f, 1.0f, 1.0f};

        RenderStageType stage{};
    };

    class RenderResource {
    public:
        void submit(const QuadDrawContext& context);
        void submit(const LineDrawContext& context);
        void submit(const TextDrawContext& context);

        [[nodiscard]] std::vector<QuadDrawContext> gain_quad_draw_contexts();

    private:
        std::vector<QuadDrawContext> texture_draw_contexts_{};
        std::vector<LineDrawContext> line_draw_contexts_{};
        std::vector<TextDrawContext> text_draw_contexts_{};
    };

    extern RenderResource* g_render_resource;

} // dodoe

#endif//DODOE_DRAW_CONTEXT_H
