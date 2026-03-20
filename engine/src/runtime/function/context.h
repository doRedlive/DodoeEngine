//
// Created by GreenMuffin on 2025/11/1.
//

#ifndef DODOE_CONTEXT_H
#define DODOE_CONTEXT_H

#include "dopch.h"

#include "runtime/core/event/event.h"
#include "runtime/core/event/event_system.h"
#include "runtime/core/meta/meta_system.h"

#include "runtime/function/time/time_system.h"
#include "runtime/function/window/window.h"
#include "runtime/function/window/window_manager.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/input/input_manager.h"
#include "runtime/function/ui/ui_system.h"

#include "runtime/resource/resource_manager.h"
#include "runtime/resource/file/file_system.h"

namespace dodoe {

    class Context {
    public:
        Scope<ResourceManager>  resource_manager {nullptr};
        Scope<WindowManager>    window_manager {nullptr};
        Scope<RenderSystem>     render_system {nullptr};
        Scope<UiSystem>         ui_system {nullptr};
        Scope<TimeSystem>       time_system {nullptr};
        Scope<EventSystem>      event_system {nullptr};
        Scope<InputManager>     input_manager {nullptr};

        Context();
        ~Context();

        [[nodiscard]] bool initialize_systems();
        [[nodiscard]] bool shutdown_systems();
    };


    extern Context g_context;
} // dodoe

#endif //DODOE_CONTEXT_H