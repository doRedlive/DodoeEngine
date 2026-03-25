//
// Created by Redlive on 2026/3/18.
//

#ifndef DODOE_RENDER_CONTEXT_H
#define DODOE_RENDER_CONTEXT_H

#include "dopch.h"

#include "GLFW/glfw3.h"

namespace dodoe {

    struct RenderContextCreateInfo {
        GLFWwindow* window{nullptr};
    };

    class RenderContext {
    public:
        virtual ~RenderContext() = default;
        
        static Scope<RenderContext> create(RenderContextCreateInfo create_info);
        static void destroy(Scope<RenderContext>& render_context);
    protected:
        virtual void initialize(RenderContextCreateInfo create_info) = 0;
        virtual void shutdown() = 0;
    };

} // dodoe

#endif//DODOE_RENDER_CONTEXT_H