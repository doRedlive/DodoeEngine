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
        Scope<Window> window_;
    public:
        void initialize(const WindowManagerInitInfo& init_info);
        void shutdown();

        [[nodiscard]] Window* window() const { return window_.get(); };
        void swapBuffers();

    private:
        void onWindowFocus(const WindowFocusEvent& event);
        void onWindowLostFocus(const WindowLostFocusEvent& event);
        void onWindowClose(const WindowCloseEvent& event);
        void onWindowResize(const WindowResizeEvent& event);
        void bindEventCallback();
    };
}


#endif //DODOE_WINDOWMANAGER_H
