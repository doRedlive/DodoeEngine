//
// Created by GreenMuffin on 2025/11/24.
//

#include "event_system.h"

#include "GLFW/glfw3.h"
#include "backends/imgui_impl_glfw.h"

namespace dodoe {

    static Scope<entt::dispatcher> g_event_dispatcher{};

    entt::dispatcher& EventSystem::dispatcher_() {
        DoAssert(g_event_dispatcher, "EventSystem not initialized");
        return *g_event_dispatcher;
    }

    void EventSystem::initialize() {
        g_event_dispatcher = create_scope<entt::dispatcher>();
    }

    void EventSystem::poll_events() {
        glfwPollEvents();
    }

    void EventSystem::handle_events() {
        dispatcher_().update();
    }

    void EventSystem::shutdown() {
        g_event_dispatcher.reset();
    }

} // dodoe
