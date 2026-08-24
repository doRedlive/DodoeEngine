// do@Redlive

#pragma once

#include "dopch.h"

#include "window_types.h"

#if defined(DO_PLATFORM_WINDOWS)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(DO_PLATFORM_MACOS)
#define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(DO_PALTFORM_LINUX)
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
#endif // DO_PLATFORMS

#include "entt/entt.hpp"
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

namespace dodoe {

    class DODOE_API Window : public Managed<Window, WindowManagerCreateInfo> {
        friend class Managed<Window, WindowManagerCreateInfo>;
        friend class WindowManager;

        WindowProperty m_prop{};
        Vector2i m_host_pixel_size{1, 1};

        GLFWwindow* m_glfw_window{nullptr};
        void* m_host_handle{nullptr};
    public:
        [[nodiscard]] bool initialize(const WindowManagerCreateInfo& info);
        void shutdown();
        void swapBuffers();

        [[nodiscard]] GLFWwindow* getNativeWindow() const { return m_glfw_window; }
        [[nodiscard]] void* getNativeHandle() const;
        [[nodiscard]] bool isHostMode() const { return m_host_handle != nullptr; }
        [[nodiscard]] Int getWidth() const { return m_prop.width; }
        [[nodiscard]] Int getHeight() const { return m_prop.height; }
        [[nodiscard]] Vector2i getPixelSize() const;
        [[nodiscard]] bool isMaximized() const;

        void maximize();
        void restore();
        void setSize(Int width, Int height);
        void setPixelSize(Int width, Int height);
    };

} // dodoe
