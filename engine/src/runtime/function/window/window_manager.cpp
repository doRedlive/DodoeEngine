// do@Redlive

#include "window_manager.h"
#include "runtime/core/event/event.h"
#include "runtime/core/event/event_system.h"
#include "GLFW/glfw3.h"

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
