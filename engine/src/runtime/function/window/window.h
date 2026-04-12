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

#include "viewport_manager.h"

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

    class Window {
        friend class WindowManager;
        GLFWwindow* window_ {nullptr};
        WindowProperty prop_ {};
    public:
        Scope<ViewportManager> viewport_manager{};

        static Scope<Window> create(const WindowProperty& prop);
        static void destroy(Scope<Window>& window);

        void initialize(const WindowProperty& prop);
        void shutdown();
        void swapBuffers();

        [[nodiscard]] GLFWwindow* nativeWindow() const { return window_;}
        [[nodiscard]] HWND handle() const;
        [[nodiscard]] uint width() const { return prop_.width; }
        [[nodiscard]] uint height() const { return prop_.height; }
        [[nodiscard]] bool is_maximized() const;
        void maximize();
        void restore();
    };

} // dodoe



#endif //DODOE_WINDOW_H
