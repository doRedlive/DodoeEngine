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

#include "runtime/function/ui/ui_system.h"
#include "runtime/function/window/window_manager.h"

namespace dodoe {

    struct RenderSystemCreateInfo {
        WindowManager* window_manager;
        UiSystem* ui_system;
        RenderApiType backend_api;
    };

    class RenderSystem {
        Scope<RhiContext> rhi_backend_{nullptr};
        Scope<Camera> camera_;
        Scope<RenderGraph> render_graph_{nullptr};
        uint32_t current_swapchain_image_index_{0};

        WindowManager* window_manager_{nullptr};
        UiSystem* ui_system_{nullptr};
    public:
        static Scope<RenderSystem> create(const RenderSystemCreateInfo& create_info);
        static void destroy(Scope<RenderSystem>& system);
        void initialize(const RenderSystemCreateInfo& init_info);
        void shutdown();

        void prepare();
        void present();
        
        [[nodiscard]] Camera& camera() { return *camera_.get(); }
        [[nodiscard]] RhiContext* rhiBackend() const { return rhi_backend_.get(); }
        [[nodiscard]] const std::vector<rhi::TextureHandle>& mainSceneTextures() const;
        [[nodiscard]] uint32_t currentSwapchainImageIndex() const { return current_swapchain_image_index_; }
        
    private:
        void swapLogicRenderContext();
    };

}

#endif//DODOE_RENDER_SYSTEM_H
