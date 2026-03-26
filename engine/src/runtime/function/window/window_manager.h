//
// Created by GreenMuffin on 2025/11/22.
//

#ifndef DODOE_WINDOWMANAGER_H
#define DODOE_WINDOWMANAGER_H
#include "dopch.h"
#include "window.h"

#include "runtime/core/application.h"

#include "runtime/core/event/event.h"

namespace dodoe {

    struct WindowManagerInitInfo {
        ApplicationSpecification spec;
    };
    
    class WindowManager {
    public:
        WindowManager();
        ~WindowManager();

        [[nodiscard]] Window* active_window() const;
        Window* create_window(const WindowProperty& props);

        [[nodiscard]] bool initialize(WindowManagerInitInfo init_info);
        void swap_buffers();
        void shutdown();

    private:
        Window* active_window_ {nullptr};
        std::vector<Scope<Window>> windows_ {};

        [[nodiscard]] Window* get_window_(uint32_t window_id) const;

        void on_window_focus_(const WindowFocusEvent& event);
        void on_window_lost_focus_(const WindowLostFocusEvent& event);
        void on_window_close_(const WindowCloseEvent& event);
        void on_window_resize_(const WindowResizeEvent& event);

        void bind_events_callback_(Window* window);
    };
}


#endif //DODOE_WINDOWMANAGER_H
