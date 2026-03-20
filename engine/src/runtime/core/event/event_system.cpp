//
// Created by GreenMuffin on 2025/11/24.
//

#include "event_system.h"

#include "GLFW/glfw3.h"
#include "backends/imgui_impl_glfw.h"

namespace dodoe {

    void EventSystem::initialize() {
        event_dispatcher_ = create_scope<entt::dispatcher>();
    }

    void EventSystem::update() {
        glfwPollEvents();
    }

    void EventSystem::handle_events() const {
        event_dispatcher_->update();
    }

    void EventSystem::shutdown() {
        event_dispatcher_ = nullptr;
    }

} // dodoe
