// do@Redlive

#pragma once

#include "dopch.h"

#include "window_types.h"
#include "window.h"

#include "runtime/core/application.h"
#include "runtime/core/event/event.h"
#include "runtime/core/memory/managed.h"

namespace dodoe {

    class WindowManager : public Managed<WindowManager, WindowManagerCreateInfo> {
        friend class Managed<WindowManager, WindowManagerCreateInfo>;

        Scope<Window> m_window;
    public:
        [[nodiscard]] Window* getWindow() const { return m_window.get(); };
        void swapBuffers();

    private:
        [[nodiscard]] Bool initialize(const WindowManagerCreateInfo& info);
        void shutdown();

        void onWindowFocus(const WindowFocusEvent& event);
        void onWindowLostFocus(const WindowLostFocusEvent& event);
        void onWindowClose(const WindowCloseEvent& event);
        void onWindowResize(const WindowResizeEvent& event);
        void bindEventCallback();
    };

} // dodoe
