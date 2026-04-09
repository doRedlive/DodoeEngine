//
// Created by GreenMuffin on 2026/3/6.
// Knight!
//

#include "render_system.h"

#include "render_api.h"
#include "render_resource.h"

#include "runtime/core/utils/util.h"

namespace dodoe {

    Scope<RenderSystem> RenderSystem::create(const RenderSystemCreateInfo& create_info) {
        auto context = create_scope<RenderSystem>();
        context->initialize(create_info);
        return context;
    }

    void RenderSystem::initialize(const RenderSystemCreateInfo& init_info) {
        window_manager_ = init_info.window_manager;
        auto window = window_manager_->active_window();

        RenderApi::initialize({init_info.backend_api});

        rhi_backend_ = RhiBackend::create({window->native_window(), init_info.backend_api, true});

        TextureManager::self().initialize(rhi_backend_->getDevice());

        camera_ = Camera::create({CameraType::Orthographic, window->viewport_manager->get_logical_size(), window->viewport_manager->get_window_size()});
        std::vector<rhi::TextureHandle> swapchain_targets{};
        Vector2i target_extent{0, 0};
        if (rhi_backend_) {
            swapchain_targets = rhi_backend_->getSwapchainTextures();
            target_extent = rhi_backend_->getSwapchainExtent2d();
        }

        render_graph_ = RenderGraph::create({
            rhi_backend_ ? rhi_backend_->getDevice() : rhi::DeviceHandle{},
            swapchain_targets,
            target_extent
        });

        render_graph_->setup();
        render_graph_->compile();
    }

    void RenderSystem::shutdown() {
        RenderGraph::destroy(render_graph_);
        Camera::destroy(camera_);
        RhiBackend::destroy(rhi_backend_);
    }

    void RenderSystem::prepare() {
        auto& viewport_manager = window_manager_->active_window()->viewport_manager;
        if (viewport_manager->dirty()) [[unlikely]] {
            viewport_manager->update();
        }

        swapLogicRenderContext();
    }

    void RenderSystem::present() {
        render_graph_->execute();
        window_manager_->swapBuffers();
    }

    void RenderSystem::swapLogicRenderContext() {
        g_RenderResource->swapLogicRenderContext();
    }

} // dodoe

