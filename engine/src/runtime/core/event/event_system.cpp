// do@Redlive

#include "event_system.h"

#include "GLFW/glfw3.h"

namespace dodoe {

    static Scope<entt::dispatcher> g_EventDispatcher{};

    entt::dispatcher& EventSystem::GetDispatcher() {
        DO_ASSERT(g_EventDispatcher, "EventSystem not initialized");
        return *g_EventDispatcher;
    }

    void EventSystem::Initialize() {
        g_EventDispatcher = create_scope<entt::dispatcher>();
    }

    void EventSystem::Poll() {
        DO_PROFILE_SCOPE_CATEGORY("EventSystem::Poll", "frame");
        glfwPollEvents();
    }

    void EventSystem::Handle() {
        DO_PROFILE_SCOPE_CATEGORY("EventSystem::Handle", "frame");
        GetDispatcher().update();
    }

    void EventSystem::Shutdown() {
        g_EventDispatcher.reset();
    }

} // dodoe
