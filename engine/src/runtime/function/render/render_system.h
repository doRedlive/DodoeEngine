//
// Created by GreenMuffin on 2026/3/6.
// love u forever. owo
//

#ifndef DODOE_RENDER_SYSTEM_H
#define DODOE_RENDER_SYSTEM_H

#include "dopch.h"

#include "runtime/function/render/backend/render_context.hpp"
#include "renderer.h"
#include "render_graph.h"

#include "runtime/function/window/window_manager.h"

namespace dodoe {

    struct RenderSystemCreateInfo {
        WindowManager* window_manager;
    };

    class RenderSystem {
    public:
        static Scope<RenderSystem> create(RenderSystemCreateInfo create_info);

        void initialize(RenderSystemCreateInfo init_info);
        void shutdown();

        void prepare();
        void present();

        [[nodiscard]] Renderer* renderer() { return renderer_.get(); }

    private:
        Scope<Renderer> renderer_;
        Scope<RenderGraph> render2d_graph_;
        Scope<RenderContext> render_context_;

        WindowManager* window_manager_;
    };


}

#endif//DODOE_RENDER_SYSTEM_H
