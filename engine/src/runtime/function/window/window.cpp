// do@Redlive

#include "window.h"

#include "runtime/resource/resource_manager.h"
#include "runtime/core/utils/common.h"

namespace dodoe {

    Bool Window::initialize(const WindowManagerCreateInfo& info) {
        DO_PROFILE_SCOPE_CATEGORY("Window::initialize", "startup");
        m_prop = info.prop;
        m_host_pixel_size = Vector2i(m_prop.width, m_prop.height);
        m_host_handle = info.host_handle;
        if (isHostMode()) {
            m_glfw_window = nullptr;
            return true;
        }

        if (m_prop.backend_api == RenderBackendApiType::OpenGL) {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        } else {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        }
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        m_glfw_window = glfwCreateWindow(m_prop.width, m_prop.height, m_prop.title, nullptr, nullptr);
        DO_ASSERT(m_glfw_window, "The window create failed!");

        Int fb_width, fb_height;
        glfwGetFramebufferSize(m_glfw_window, &fb_width, &fb_height);

        return m_glfw_window != nullptr;
    }

    void Window::swapBuffers() {
        if (isHostMode()) return;
        if (m_glfw_window) glfwSwapBuffers(m_glfw_window);
    }

    void Window::shutdown() {
        DO_PROFILE_SCOPE_CATEGORY("Window::shutdown", "shutdown");
        if (isHostMode()) {
            m_host_handle = nullptr;
            return;
        }
        if (m_glfw_window) {
            glfwDestroyWindow(m_glfw_window);
            m_glfw_window = nullptr;
        }
    }

    void* Window::getNativeHandle() const {
        if (isHostMode()) return m_host_handle;
        if (!m_glfw_window) return nullptr;
#if defined(DO_PLATFORM_WINDOWS)
        return static_cast<void*>(glfwGetWin32Window(m_glfw_window));
#elif defined(DO_PLATFORM_MACOS)
        return static_cast<void*>(glfwGetCocoaWindow(window_));
#elif defined(DO_PLATFORM_LINUX)
        return static_cast<void*>(glfwGetX11Window(window_));
#else
        return nullptr;
#endif
    }

    void Window::setSize(Int width, Int height) {
        m_prop.width = width;
        m_prop.height = height;
        if (!isHostMode() && m_glfw_window) glfwSetWindowSize(m_glfw_window, width, height);
    }

    void Window::setPixelSize(Int width, Int height) {
        if (isHostMode()) {
            m_host_pixel_size = Vector2i(width, height);
        }
    }

    Vector2i Window::getPixelSize() const {
        if (isHostMode()) return m_host_pixel_size;
        Int w = 0, h = 0;
        glfwGetFramebufferSize(m_glfw_window, &w, &h);
        return Vector2i(w, h);
    }

    Bool Window::isMaximized() const {
        if (isHostMode()) return false;
        return static_cast<Bool>(glfwGetWindowAttrib(m_glfw_window, GLFW_MAXIMIZED));
    }

    void Window::maximize() {
        if (isHostMode() || isMaximized()) return;
        glfwMaximizeWindow(m_glfw_window);
    }

    void Window::restore() {
        if (!isHostMode()) glfwRestoreWindow(m_glfw_window);
    }

}
