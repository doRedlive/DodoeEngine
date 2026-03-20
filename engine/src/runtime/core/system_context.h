//
// Created by GreenMuffin on 2025/11/1.
//

#ifndef DODOE_SYSTEM_CONTEXT_H
#define DODOE_SYSTEM_CONTEXT_H

#include "dopch.h"

namespace dodoe {

    class WindowManager;
    class RenderSystem;
    class UiSystem;
    class TimeSystem;
    class EventSystem;
    class InputManager;
    
    class SystemContext {
    public:
        SystemContext();
        ~SystemContext();

        Scope<WindowManager>    window_manager {nullptr};
        Scope<RenderSystem>     render_system  {nullptr};
        Scope<InputManager>     input_manager  {nullptr};
        Scope<EventSystem>      event_system   {nullptr};
        Scope<TimeSystem>       time_system    {nullptr};
        Scope<UiSystem>         ui_system      {nullptr};

        [[nodiscard]] bool initialize_systems();
        [[nodiscard]] bool shutdown_systems();
    };

} // dodoe

#endif //DODOE_SYSTEM_CONTEXT_H
