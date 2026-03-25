//
// Created by Redlive on 2026/3/18.
//

#ifndef DODOE_GL_RENDER_CONTEXT_H
#define DODOE_GL_RENDER_CONTEXT_H

#include "dopch.h"

#include "runtime/function/render/backend/render_context.h"

#include "glad/glad.h"
#include "GLFW/glfw3.h"

namespace dodoe {

    class GlRenderContext : public RenderContext {
    protected:
        void initialize(RenderContextCreateInfo create_info);
        void shutdown();
    };

} // dodoe

#endif//DODOE_GL_RENDER_CONTEXT_H