//
// Created by GreenMuffin on 2025/10/16.
//

#include "window.h"

#include "runtime/resource/resource_manager.h"
#include "runtime/core/utils/common.h"


namespace {
    GLFWmonitor* get_best_monitor_for_window(GLFWwindow* window) {
        if (!window) {
            return glfwGetPrimaryMonitor();
        }

        int monitor_count = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);
        if (!monitors || monitor_count == 0) {
            return glfwGetPrimaryMonitor();
        }

        int wx = 0;
        int wy = 0;
        int ww = 0;
        int wh = 0;
        glfwGetWindowPos(window, &wx, &wy);
        glfwGetWindowSize(window, &ww, &wh);

        int best_overlap = -1;
        GLFWmonitor* best_monitor = nullptr;

        for (int i = 0; i < monitor_count; ++i) {
            GLFWmonitor* monitor = monitors[i];
            int mx = 0;
            int my = 0;
            glfwGetMonitorPos(monitor, &mx, &my);
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            if (!mode) {
                continue;
            }

            const int mw = mode->width;
            const int mh = mode->height;

            const int overlap_width = (std::max)(0, (std::min)(wx + ww, mx + mw) - (std::max)(wx, mx));
            const int overlap_height = (std::max)(0, (std::min)(wy + wh, my + mh) - (std::max)(wy, my));
            const int overlap = overlap_width * overlap_height;

            if (overlap > best_overlap) {
                best_overlap = overlap;
                best_monitor = monitor;
            }
        }

        if (best_monitor) {
            return best_monitor;
        }

        return glfwGetPrimaryMonitor();
    }
}

namespace dodoe {

    Window::Window(const WindowProperty& props) : prop_(props) {}

    Window::~Window() = default;

    bool Window::initialize() {
        if (prop_.backend_api == RenderApiType::OpenGL) {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        }
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        if (prop_.resizeable) glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        if (prop_.custom_titlebar) glfwWindowHint(GLFW_TITLEBAR, GLFW_FALSE);

        window_ = glfwCreateWindow(prop_.width, prop_.height, prop_.title, nullptr, nullptr);
        DoAssert(window_, "The window create failed!");

        int fb_width, fb_height;
        glfwGetFramebufferSize(window_, &fb_width, &fb_height);

        viewport_manager = ViewportManager::create({Vector2f(640.0f, 360.0f), Vector2f(prop_.width, prop_.height), Vector2f(fb_width, fb_height)});

        data_.title = prop_.title;
        data_.id = string2hash(data_.title);
        data_.owner = this;
        glfwSetWindowUserPointer(window_, &data_);

        if (prop_.custom_titlebar) {
            glfwSetTitlebarHitTestCallback(window_, [](GLFWwindow* native_window, int x, int y, int* hit) {
                (void)x;
                (void)y;
                WindowData& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(native_window));
                *hit = (data.owner != nullptr && data.owner->is_titlebar_hovered()) ? 1 : 0;
            });
        }
        
        DoInfo("The window crate success.");
        return true;
    }

    void Window::swap_buffer() {
        glfwSwapBuffers(window_);
    }

    void Window::shutdown() {
        ViewportManager::destroy(viewport_manager);
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

    bool Window::is_maximized() const {
        return borderless_maximized_ || static_cast<bool>(glfwGetWindowAttrib(window_, GLFW_MAXIMIZED));
    }

    void Window::maximize() {
        if (!window_ || is_maximized()) {
            return;
        }

        if (prop_.custom_titlebar) {
            maximize_borderless_();
            return;
        }

        glfwMaximizeWindow(window_);
    }

    void Window::restore() {
        if (!window_) {
            return;
        }

        if (borderless_maximized_) {
            restore_borderless_();
            return;
        }

        glfwRestoreWindow(window_);
    }

    void Window::toggle_maximize() {
        if (is_maximized()) {
            restore();
        } else {
            maximize();
        }
    }

    void Window::maximize_borderless_() {
        if (!window_ || borderless_maximized_) {
            return;
        }

        glfwGetWindowPos(window_, &windowed_rect_.x, &windowed_rect_.y);
        glfwGetWindowSize(window_, &windowed_rect_.width, &windowed_rect_.height);

        GLFWmonitor* monitor = get_best_monitor_for_window(window_);

        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        if (monitor) {
            glfwGetMonitorWorkarea(monitor, &x, &y, &width, &height);

            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            int monitor_x = 0;
            int monitor_y = 0;
            glfwGetMonitorPos(monitor, &monitor_x, &monitor_y);
            if (mode != nullptr &&
                x == monitor_x && y == monitor_y &&
                width == mode->width && height == mode->height) {
                width = (width > 1) ? (width - 1) : 1;
                height = (height > 1) ? (height - 1) : 1;
            }
        } else {
            glfwGetWindowPos(window_, &x, &y);
            glfwGetWindowSize(window_, &width, &height);
        }

        glfwSetWindowPos(window_, x, y);
        glfwSetWindowSize(window_, width, height);
        borderless_maximized_ = true;
    }

    void Window::restore_borderless_() {
        if (!window_ || !borderless_maximized_) {
            return;
        }

        glfwSetWindowPos(window_, windowed_rect_.x, windowed_rect_.y);
        glfwSetWindowSize(window_, windowed_rect_.width, windowed_rect_.height);
        borderless_maximized_ = false;
    }
}
