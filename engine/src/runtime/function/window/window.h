//
// Created by GreenMuffin on 2025/10/16.
//

#ifndef DODOE_WINDOW_H
#define DODOE_WINDOW_H

#include "dopch.h"

#if defined(DO_PLATFORM_WINDOWS)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(DO_PLATFORM_MACOS)
#define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(DO_PALTFORM_LINUX)
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
#endif // DO_PLATFORMS

#include "runtime/function/render/render_api.h"

#include "entt/entt.hpp"

#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

namespace dodoe {

    struct WindowProperty {
        int width;
        int height;
        const char* title;
        bool custom_titlebar;
        bool resizeable;
        RenderApiType backend_api;

        explicit WindowProperty(const int w = 800, const int h = 600, const char* t = "dodoe", const bool custom_titlebar = false, const bool resizeable = true)
            : width(w), height(h), title(t), custom_titlebar(custom_titlebar), resizeable(resizeable) , backend_api(RenderApiType::None) {}
    };

    class Window : public Managed<Window, WindowProperty> {
        friend class Managed<Window, WindowProperty>;
        friend class WindowManager;
        GLFWwindow* window_ {nullptr};
        WindowProperty prop_ {};
    public:

        [[nodiscard]] bool initialize(const WindowProperty& prop);
        void shutdown();
        void swapBuffers();

        [[nodiscard]] GLFWwindow* getNativeWindow() const { return window_;}
        [[nodiscard]] HWND handle() const;
        [[nodiscard]] int width() const { return prop_.width; }
        [[nodiscard]] int height() const { return prop_.height; }
        [[nodiscard]] Vector2i pixelSize() const;
        [[nodiscard]] bool is_maximized() const;
        void maximize();
        void restore();
    };

} // dodoe



#endif //DODOE_WINDOW_H
