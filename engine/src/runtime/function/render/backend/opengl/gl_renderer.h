//
// Created by Redlive on 2026/3/18.
//

#ifndef DODOE_GL_RENDERER_H
#define DODOE_GL_RENDERER_H

#include "dopch.h"

#include "gl_render_context.h"

#include "runtime/function/render/renderer.h"
#include "runtime/function/render/backend/texture.h"

namespace dodoe {

    class GlRenderer : public Renderer {
    public:
        void draw_sprite(const Ref<Texture>& texture, const Vector2f& pos, 
            const Vector2f& size, const Vector3f& rotation, const Vector4f& color);

        void draw_line();
        void draw_text();

    protected:
        void initialize(RendererCreateInfo create_info);
        void shutdown();
    };
    
} // dodoe

#endif//DODOE_GL_RENDERER_H