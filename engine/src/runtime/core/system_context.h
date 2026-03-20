//
// Created by GreenMuffin on 2025/11/1.
//

#ifndef DODOE_SYSTEM_CONTEXT_H
#define DODOE_SYSTEM_CONTEXT_H

#include "dopch.h"

#include "runtime/core/layer/layer_stack.h"

namespace dodoe {

    class WindowManager;
    class RenderSystem;
    class UiSystem;
    class TimeSystem;
    class EventSystem;
    class InputManager;
 
    class SystemContext {
    public:
        Scope<WindowManager>    window_manager {nullptr};
        Scope<RenderSystem>     render_system  {nullptr};
        Scope<InputManager>     input_manager  {nullptr};
        Scope<EventSystem>      event_system   {nullptr};
        Scope<TimeSystem>       time_system    {nullptr};
        Scope<UiSystem>         ui_system      {nullptr};

        LayerStack layer_stack{};

        static Scope<SystemContext> create();
        static void destroy(Scope<SystemContext>& context);

        [[nodiscard]] bool initialize_systems();
        [[nodiscard]] bool shutdown_systems();

        void tick_one_frame();
    
    private:

        void update_tick(float dt);
        void render_tick();
    };

} // dodoe

#endif //DODOE_SYSTEM_CONTEXT_H
