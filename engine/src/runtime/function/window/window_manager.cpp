// do@Redlive

#include "window_manager.h"
#include "runtime/core/event/event.h"
#include "runtime/core/event/event_system.h"
#include "runtime/function/input/key_code.h"
#include "runtime/function/input/mouse_code.h"
#include "GLFW/glfw3.h"

#ifdef DODOE_DEBUG_ENABLED
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#endif

namespace dodoe {

    bool WindowManager::initialize(const WindowManagerCreateInfo& init_info) {
        DO_PROFILE_SCOPE_CATEGORY("WindowManager::initialize", "startup");

        EventSystem::Subscribe<WindowFocusEvent, &WindowManager::onWindowFocus>(this);
        EventSystem::Subscribe<WindowCloseEvent, &WindowManager::onWindowClose>(this);
        EventSystem::Subscribe<WindowResizeEvent, &WindowManager::onWindowResize>(this);
        EventSystem::Subscribe<WindowLostFocusEvent, &WindowManager::onWindowLostFocus>(this);

        glfwInit();
        WindowManagerCreateInfo window_create_info{init_info.host_handle, init_info.prop};
        m_window = Window::Create(window_create_info);
        if (!init_info.host_handle) {
            bindEventCallback();
        }

        return true;
    }

    void WindowManager::shutdown() {
        DO_PROFILE_SCOPE_CATEGORY("WindowManager::shutdown", "shutdown");
        EventSystem::Unsubscribe<WindowFocusEvent, &WindowManager::onWindowFocus>(this);
        EventSystem::Unsubscribe<WindowLostFocusEvent, &WindowManager::onWindowLostFocus>(this);
        EventSystem::Unsubscribe<WindowCloseEvent, &WindowManager::onWindowClose>(this);
        EventSystem::Unsubscribe<WindowResizeEvent, &WindowManager::onWindowResize>(this);
        Window::Destroy(m_window);
        glfwTerminate();
    }

    void WindowManager::swapBuffers() {
        m_window->swapBuffers();
    }

    void WindowManager::bindEventCallback() {
        glfwSetWindowSizeCallback(m_window->getNativeWindow(), [](GLFWwindow* native_window, int width, int height) {
            WindowResizeEvent event(width, height);
            EventSystem::Enqueue<WindowResizeEvent>(event);
        });
        glfwSetFramebufferSizeCallback(m_window->getNativeWindow(), [](GLFWwindow* native_window, int width, int height) {
            (void)native_window; (void)width; (void)height;
        });
        glfwSetWindowCloseCallback(m_window->getNativeWindow(), [](GLFWwindow* native_window) {
            WindowCloseEvent event;
            EventSystem::Enqueue<WindowCloseEvent>(event);
        });
        glfwSetWindowFocusCallback(m_window->getNativeWindow(), [](GLFWwindow* native_window, int focused) {
            if (static_cast<bool>(focused)) {
                WindowFocusEvent event;
                EventSystem::Enqueue<WindowFocusEvent>(event);
            } else {
                WindowLostFocusEvent event;
                EventSystem::Enqueue<WindowLostFocusEvent>(event);
            }
        });
        glfwSetKeyCallback(m_window->getNativeWindow(), [](GLFWwindow* native_window, int key, int scancode, int action, int mods) {
#ifdef DODOE_DEBUG_ENABLED
            if (ImGui::GetCurrentContext()) ImGui_ImplGlfw_KeyCallback(native_window, key, scancode, action, mods);
#endif
            switch (action) {
                case GLFW_PRESS: { KeyPressedEvent event(static_cast<KeyCode>(key), false); EventSystem::Enqueue<KeyPressedEvent>(event); break; }
                case GLFW_RELEASE: { KeyReleasedEvent event(static_cast<KeyCode>(key)); EventSystem::Enqueue<KeyReleasedEvent>(event); break; }
                case GLFW_REPEAT: { KeyPressedEvent event(static_cast<KeyCode>(key), true); EventSystem::Enqueue<KeyPressedEvent>(event); break; }
            }
        });
        glfwSetCharCallback(m_window->getNativeWindow(), [](GLFWwindow* native_window, unsigned int keycode) {
#ifdef DODOE_DEBUG_ENABLED
            if (ImGui::GetCurrentContext()) ImGui_ImplGlfw_CharCallback(native_window, keycode);
#endif
        });
        glfwSetMouseButtonCallback(m_window->getNativeWindow(), [](GLFWwindow* native_window, int button, int action, int mods) {
#ifdef DODOE_DEBUG_ENABLED
            if (ImGui::GetCurrentContext()) ImGui_ImplGlfw_MouseButtonCallback(native_window, button, action, mods);
#endif
            switch (action) {
                case GLFW_PRESS: { MouseButtonPressedEvent event(static_cast<MouseCode>(button)); EventSystem::Enqueue<MouseButtonPressedEvent>(event); break; }
                case GLFW_RELEASE: { MouseButtonReleasedEvent event(static_cast<MouseCode>(button)); EventSystem::Enqueue<MouseButtonReleasedEvent>(event); break; }
            }
        });
        glfwSetScrollCallback(m_window->getNativeWindow(), [](GLFWwindow* native_window, double x_offset, double y_offset) {
#ifdef DODOE_DEBUG_ENABLED
            if (ImGui::GetCurrentContext()) ImGui_ImplGlfw_ScrollCallback(native_window, x_offset, y_offset);
#endif
            MouseScrolledEvent event(static_cast<float>(x_offset), static_cast<float>(y_offset));
            EventSystem::Enqueue<MouseScrolledEvent>(event);
        });
        glfwSetCursorPosCallback(m_window->getNativeWindow(), [](GLFWwindow* native_window, double x_pos, double y_pos) {
#ifdef DODOE_DEBUG_ENABLED
            if (ImGui::GetCurrentContext()) ImGui_ImplGlfw_CursorPosCallback(native_window, x_pos, y_pos);
#endif
            MouseMovedEvent event(static_cast<float>(x_pos), static_cast<float>(y_pos));
            EventSystem::Enqueue<MouseMovedEvent>(event);
        });
    }

    void WindowManager::onWindowFocus(const WindowFocusEvent& event) {}
    void WindowManager::onWindowLostFocus(const WindowLostFocusEvent& event) {}

    void WindowManager::onWindowClose(const WindowCloseEvent& event) {
        if (m_window && !m_window->isHostMode() && m_window->getNativeWindow())
            glfwSetWindowShouldClose(m_window->getNativeWindow(), GLFW_TRUE);
        EventSystem::Publish<ApplicationQuitEvent>();
    }

    void WindowManager::onWindowResize(const WindowResizeEvent& event) {
        m_window->m_prop.width = static_cast<uint>(event.width);
        m_window->m_prop.height = static_cast<uint>(event.height);
        if (m_window->isHostMode() || !m_window->getNativeWindow()) return;
        int fb_width = 0, fb_height = 0;
        glfwGetFramebufferSize(m_window->getNativeWindow(), &fb_width, &fb_height);
    }

} // dodoe
