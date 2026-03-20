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

    class Window;

    struct WindowData {
        std::string title;
        uint32_t id;
        Window* owner{nullptr};
    };

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
    public:
        explicit Window(const WindowProperty& prop);
        ~Window();

        [[nodiscard]]
        bool initialize();
        void shutdown();
        void swap_buffer();

        [[nodiscard]] GLFWwindow* native_window() const { return window_;}
        [[nodiscard]] HWND handle() const;
        [[nodiscard]] uint width() const { return prop_.width; }
        [[nodiscard]] uint height() const { return prop_.height; }
        [[nodiscard]] bool is_maximized() const;
        [[nodiscard]] bool is_titlebar_hovered() const { return titlebar_hovered_; }
        void set_titlebar_hovered(const bool hovered) { titlebar_hovered_ = hovered; }
        void maximize();
        void restore();
        void toggle_maximize();

    private:
        struct WindowRect {
            int x = 0;
            int y = 0;
            int width = 0;
            int height = 0;
        };

        void maximize_borderless_();
        void restore_borderless_();

        GLFWwindow* window_ {nullptr};
        WindowProperty prop_ {};
        WindowData data_{};
        bool borderless_maximized_ {false};
        bool titlebar_hovered_{false};
        WindowRect windowed_rect_{};
    };

    inline Window* create_window(const WindowProperty& prop) {
        return new Window(prop);
    }
} // dodoe



#endif //DODOE_WINDOW_H