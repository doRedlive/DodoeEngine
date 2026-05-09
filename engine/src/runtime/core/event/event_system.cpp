//
// Created by GreenMuffin on 2025/11/24.
//

#include "event_system.h"

#include "GLFW/glfw3.h"

namespace dodoe {

    static Scope<entt::dispatcher> g_event_dispatcher{};

    bool EventSystem::initialized_() {
        return static_cast<bool>(g_event_dispatcher);
    }

    entt::dispatcher& EventSystem::dispatcher_() {
        DO_ASSERT(g_event_dispatcher, "EventSystem not initialized");
        return *g_event_dispatcher;
    }

    void EventSystem::initialize() {
        g_event_dispatcher = create_scope<entt::dispatcher>();
    }

    void EventSystem::Poll() {
        glfwPollEvents();
    }

    void EventSystem::Handle() {
        if (!initialized_()) {
            return;
        }
        dispatcher_().update();
    }

    void EventSystem::shutdown() {
        g_event_dispatcher.reset();
    }

} // dodoe
