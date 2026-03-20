//
// Created by GreenMuffin on 2025/11/1.
//

#include "dopch.h"

#include "system_context.h"

#include "runtime/core/event/event.h"
#include "runtime/core/event/event_system.h"

#include "runtime/function/time/time_system.h"
#include "runtime/function/window/window.h"
#include "runtime/function/window/window_manager.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/input/input_manager.h"
#include "runtime/function/ui/ui_system.h"

#include "runtime/core/world/world_manager.h"

#include "runtime/resource/resource_manager.h"
#include "runtime/resource/file/file_system.h"

namespace dodoe {

    SystemContext::SystemContext() = default;
    SystemContext::~SystemContext() = default;

    bool SystemContext::initialize_systems() {
        window_manager = create_scope<WindowManager>();
        render_system = create_scope<RenderSystem>();
        time_system = create_scope<TimeSystem>();
        ui_system = create_scope<UiSystem>();
        event_system = create_scope<EventSystem>();
        input_manager = create_scope<InputManager>();

        Log::initialize();
        event_system->initialize();
        input_manager->initialize();
        if (!window_manager->initialize()) return false;
        ResourceManager::self().initialize();
        render_system->initialize({window_manager.get()});
        ui_system->initialize();

        WorldManager::self().initialize({render_system->renderer()});
 
        return true;
    }

    bool SystemContext::shutdown_systems() {

        WorldManager::self().shutdown();

        ui_system->shutdown();
        ui_system.reset();
        ResourceManager::self().shutdown();
        time_system.reset();
        render_system->shutdown();
        render_system.reset();
        window_manager->shutdown();
        window_manager.reset();
        input_manager->shutdown();
        input_manager.reset();
        event_system->shutdown();
        event_system.reset();
        return true;
    }

}
