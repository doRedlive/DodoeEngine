//
// Created by GreenMuffin on 2025/11/1.
//

#include "dopch.h"

#include "context.h"

namespace dodoe {
    Context g_context;

    Context::Context() {
        MetaSystem::initialize();
    }

    Context::~Context() = default;

    bool Context::initialize_systems() {
        window_manager = create_scope<WindowManager>();
        render_system = create_scope<RenderSystem>();
        time_system = create_scope<TimeSystem>();
        resource_manager = create_scope<ResourceManager>();
        ui_system = create_scope<UiSystem>();
        event_system = create_scope<EventSystem>();
        input_manager = create_scope<InputManager>();

        Log::initialize();
        event_system->initialize();
        input_manager->initialize();
        if (!window_manager->initialize()) return false;
        resource_manager->initialize();
        render_system->initialize({window_manager.get()});
        ui_system->initialize();
        return true;
    }

    bool Context::shutdown_systems() {
        ui_system->shutdown();
        ui_system.reset();
        time_system.reset();
        render_system->shutdown();
        render_system.reset();
        resource_manager->shutdown();
        resource_manager.reset();
        window_manager->shutdown();
        window_manager.reset();
        input_manager->shutdown();
        input_manager.reset();
        event_system->shutdown();
        event_system.reset();
        return true;
    }

}
