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

    void RenderSystem::destroy(Scope<RenderSystem>& system) {
        if (!system) return;
        system->shutdown();
        system.reset();
    }

    void RenderSystem::initialize(const RenderSystemCreateInfo& init_info) {
        window_manager_ = init_info.window_manager;
        ui_system_ = init_info.ui_system;
        auto window = window_manager_->window();

        RenderApi::initialize({init_info.backend_api});
        rhi_backend_ = RhiContext::create({window->nativeWindow(), init_info.backend_api, true});
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
            target_extent,
            camera_.get(),
            rhi_backend_.get(),
            ui_system_
        });

        render_graph_->setup();
        render_graph_->compile();
    }

    void RenderSystem::shutdown() {
        RenderGraph::destroy(render_graph_);
        Camera::destroy(camera_);
        TextureManager::self().shutdown();
		if (rhi_backend_ && rhi_backend_->getDevice()) {
			rhi_backend_->getDevice()->waitForIdle();
			rhi_backend_->getDevice()->runGarbageCollection();
		}
        RhiContext::destroy(rhi_backend_);
    }

    void RenderSystem::prepare() {
        auto& viewport_manager = window_manager_->window()->viewport_manager;
        if (viewport_manager->dirty()) [[unlikely]] {
            viewport_manager->update();
        }

        swapLogicRenderContext();
    }

    void RenderSystem::present() {
        if (RenderApi::apiType() == RenderApiType::Vulkan && rhi_backend_) {
            uint32_t image_index = 0;
            if (!rhi_backend_->acquireNextSwapchainImage(image_index)) {
                return;
            }
            current_swapchain_image_index_ = image_index;

            render_graph_->execute(image_index);
            rhi_backend_->getDevice()->waitForIdle();
            if (!rhi_backend_->presentSwapchainImage(image_index)) {
                return;
            }
            rhi_backend_->getDevice()->runGarbageCollection();
            return;
        }

        render_graph_->execute();
        // window_manager_->swapBuffers();
    }

    const std::vector<rhi::TextureHandle>& RenderSystem::mainSceneTextures() const {
        static const std::vector<rhi::TextureHandle> empty_textures{};
        if (!render_graph_) {
            return empty_textures;
        }
        return render_graph_->getMainSceneTextures();
    }

    void RenderSystem::swapLogicRenderContext() {
        g_RenderResource->swapLogicRenderContext();
    }

} // dodoe

