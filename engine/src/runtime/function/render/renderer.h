//
// Created by Redlive on 2026/3/17.
//

#ifndef DODOE_RENDERER_H
#define DODOE_RENDERER_H

#include "dopch.h"

#include "runtime/function/render/backend/texture.h"
#include "render_graph.h"

#include "GLFW/glfw3.h"

namespace dodoe {

    struct RendererCreateInfo {
        RenderGraph* render2d_graph{nullptr};
    };

    class Renderer {
    public:
        static Scope<Renderer> create(RendererCreateInfo create_info);
        static void destroy(Scope<Renderer>& renderer);

        void initialize(RendererCreateInfo create_info);
        void shutdown();

        void draw_sprite(const Ref<Texture>& texture, const Vector2f& pos, 
            const Vector2f& size, const Vector3f& rotation, const Vector4f& color);
        void draw_line();
        void draw_text();
    private:
        RenderGraph* render2d_graph_{nullptr};
    };

} // dodoe

#endif//DODOE_RENDERER_H