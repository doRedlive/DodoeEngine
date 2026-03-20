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

    void RenderSystem::initialize(RenderSystemInitInfo init_info) {
        window_manager_ = init_info.window_manager;
        auto native_window = window_manager_->active_window()->native_window();
        RenderApi::initialize({RenderApiType::OpenGL});
        render_context_ = RenderContext::create({native_window});

        int frame_width = 1;
        int frame_height = 1;
        if (native_window) {
            glfwGetFramebufferSize(native_window, &frame_width, &frame_height);
        }

        render2d_graph_ = RenderGraph::create({
            static_cast<ui32>((std::max)(frame_width, 1)),
            static_cast<ui32>((std::max)(frame_height, 1))
        });
        renderer_ = Renderer::create({render2d_graph_.get()});
    }

    void RenderSystem::shutdown() {
        Renderer::destroy(renderer_);
        RenderGraph::destroy(render2d_graph_);
        RenderContext::destroy(render_context_);
    }

    void RenderSystem::prepare() {
        RenderDrawer::clear_color(Color::Gray());
        renderer_->draw_sprite(ResourceManager::self().load_texture("engine/res/pictures/grm.jpg"), {-0.5f, -0.5f}, {1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f});
    }

    void RenderSystem::present() {
        if (render2d_graph_ && render2d_graph_->sprite_stage) {
            render2d_graph_->sprite_stage->flush();
        }

        window_manager_->update();
    }

}

