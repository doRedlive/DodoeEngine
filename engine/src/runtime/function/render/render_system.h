//
// Created by GreenMuffin on 2026/3/6.
// love u forever. owo
//

#ifndef DODOE_RENDER_SYSTEM_H
#define DODOE_RENDER_SYSTEM_H

#include "dopch.h"

#include "render_graph.h"
#include "render_helper.h"
#include "interface/rhi_backend.h"
#include "camera/camera.h"

#include "runtime/function/window/window_manager.h"

namespace dodoe {

    struct RenderSystemCreateInfo {
        WindowManager* window_manager;
        RenderApiType backend_api;
    };

    class RenderSystem {
        Scope<RhiBackend> rhi_backend_{nullptr};
        Scope<Camera> camera_;
        Scope<RenderGraph> render_graph_{nullptr};

        WindowManager* window_manager_{nullptr};
    public:
        static Scope<RenderSystem> create(const RenderSystemCreateInfo& create_info);

        void initialize(const RenderSystemCreateInfo& init_info);
        void shutdown();

        void prepare();
        void present();
        
        [[nodiscard]] Camera& camera() { return *camera_.get(); }
        
    private:
        void swapLogicRenderContext();
    };


}

#endif//DODOE_RENDER_SYSTEM_H
