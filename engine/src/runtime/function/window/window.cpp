//
// Created by GreenMuffin on 2025/10/16.
//

#include "window.h"

#include "runtime/resource/resource_manager.h"
#include "runtime/core/utils/common.h"

namespace dodoe {

    Scope<Window> Window::create(const WindowProperty& props) {
        auto context = create_scope<Window>();
        context->initialize(props);
        return context;
    }

    void Window::destroy(Scope<Window>& window) {
        if (!window) return;
        window->shutdown();
        window.reset();
    }

    void Window::initialize(const WindowProperty& prop) {
        prop_ = prop;
        if (prop_.backend_api == RenderApiType::OpenGL) {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        } else {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        }
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        if (prop_.resizeable) glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        if (prop_.custom_titlebar) glfwWindowHint(GLFW_TITLEBAR, GLFW_FALSE);

        window_ = glfwCreateWindow(prop_.width, prop_.height, prop_.title, nullptr, nullptr);
        DO_ASSERT(window_, "The window create failed!");

        int fb_width, fb_height;
        glfwGetFramebufferSize(window_, &fb_width, &fb_height);

        if (prop_.custom_titlebar) {
            glfwSetTitlebarHitTestCallback(window_, [](GLFWwindow* native_window, int x, int y, int* hit) {});
        } 
    }

    void Window::swapBuffers() {
        glfwSwapBuffers(window_);
    }

    void Window::shutdown() {
        if (window_) {
            glfwDestroyWindow(window_);
            window_ = nullptr;
        }
    }

    HWND Window::handle() const {
#if defined (DO_PLATFORM_WINDOWS)
        return glfwGetWin32Window(window_);
#elif defined (DO_PLATFORM_MACOS)
        return glfwGetCocoaWindow(window_);
#elif defined (DO_PLATFORM_LINUX)
        return glfwGetX11Window(window_);
#endif //DO_PLATFORMS

    }

    Vector2i Window::pixelSize() const {
        int w = 0, h = 0;
        glfwGetFramebufferSize(window_, &w, &h);
        return Vector2i(w, h);
    }

    bool Window::is_maximized() const {
        return static_cast<bool>(glfwGetWindowAttrib(window_, GLFW_MAXIMIZED));
    }

    void Window::maximize() {
        if (is_maximized()) { return; }
        glfwMaximizeWindow(window_);
    }

    void Window::restore() {
        glfwRestoreWindow(window_);
    }

}
