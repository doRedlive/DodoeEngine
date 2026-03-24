//
// Created by GreenMuffin on 2026/3/6.
// Knight!
//

#include "render_system.h"

#include "render_api.h"
#include "backend/render_drawer.h"

#include "runtime/resource/resource_manager.h"
#include "runtime/core/utils/util.h"

namespace dodoe {

    Scope<RenderSystem> RenderSystem::create(RenderSystemCreateInfo create_info) {
        auto context = create_scope<RenderSystem>();
        context->initialize(create_info);
        return context;
    }

    void RenderSystem::initialize(RenderSystemCreateInfo init_info) {
        window_manager_ = init_info.window_manager;
        auto window = window_manager_->active_window();

        RenderApi::initialize({init_info.backend_api});
        render_context_ = RenderContext::create({window->native_window()});         
        camera_ = Camera::create({CameraType::Orthographic, window->viewport_manager->get_logical_size(), window->viewport_manager->get_window_size()});
        render2d_graph_ = RenderGraph::create({window->viewport_manager->get_logical_size(), camera_.get()});

        renderer_ = Renderer::create({render2d_graph_.get()});
    }

    void RenderSystem::shutdown() {
        Renderer::destroy(renderer_);
        RenderGraph::destroy(render2d_graph_);
        Camera::destroy(camera_);
        RenderContext::destroy(render_context_);
    }

    void RenderSystem::prepare() {
        auto& viewport_manager = window_manager_->active_window()->viewport_manager;
        if (viewport_manager->dirty()) [[unlikely]] {
            viewport_manager->update();
            RenderDrawer::update_viewport(viewport_manager->viewport());
        }
        RenderDrawer::clear_color(camera_->get_clear_color());
    }

    void RenderSystem::present() {
        if (render2d_graph_ && render2d_graph_->sprite_stage) {
            render2d_graph_->sprite_stage->flush();
        }

        window_manager_->update();
    }

}

