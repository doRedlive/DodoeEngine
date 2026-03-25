//
// Created by GreenMuffin on 2025/11/1.
//

#include "dopch.h"

#include "system_context.h"
// core
#include "runtime/core/event/event.h"
#include "runtime/core/event/event_system.h"
#include "runtime/core/meta/reflection/reflection_register.h"

#include "runtime/core/layer/layer.h"
#include "runtime/core/layer/layer_stack.h"

#include "runtime/function/time/time_system.h"

// resource
#include "runtime/resource/resource_manager.h"
#include "runtime/resource/file/file_system.h"

// render
#include "runtime/function/window/window.h"
#include "runtime/function/window/window_manager.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/ui/ui_system.h"

// game
#include "runtime/core/world/world_manager.h"
#include "runtime/function/input/input_manager.h"
#include "runtime/function/script/script_system.h"
#include "runtime/function/physics/physics_system.h"

namespace dodoe {

    SystemContext::~SystemContext() = default;

    Scope<SystemContext> SystemContext::create(SystemContextCreateInfo create_info) {
        auto context = create_scope<SystemContext>();
        DoAssert(context->initialize_systems(create_info), "System initialize failed!");
        return context;
    }

    void SystemContext::destroy(Scope<SystemContext>& context) {
        if (!context) return;
        DoAssert(context->shutdown_systems(), "System shutdown failed!");
        context.reset();
    }

    bool SystemContext::initialize_systems(SystemContextCreateInfo create_info) {
        window_manager = create_scope<WindowManager>();
        render_system  = create_scope<RenderSystem>();
        input_manager  = create_scope<InputManager>();
        time_system    = create_scope<TimeSystem>();
        ui_system      = create_scope<UiSystem>();
        // ---------------------CORE-------------------------
        Log::initialize();
        EventSystem::initialize();
        reflection::TypeMetaRegister::meta_register();
        // ---------------------RESOURCE-------------------------
        ResourceManager::self().initialize();
        // ---------------------RENDER-------------------------
        if (!window_manager->initialize({create_info.spec})) return false;
        render_system->initialize({window_manager.get(), create_info.spec.render_api_type});
        ui_system->initialize(window_manager.get());
        
        // ---------------------GAME-------------------------
        input_manager->initialize({window_manager->active_window()->viewport_manager.get()});
        physics_system = PhysicsSystem::create({});
        WorldManager::self().initialize({render_system.get(), physics_system.get()});
        script_system  = ScriptSystem::create({});
 
        return true;
    }

    bool SystemContext::shutdown_systems() {
        // ---------------------GAME-------------------------
        WorldManager::self().shutdown();
        ScriptSystem::destroy(script_system);
        PhysicsSystem::destroy(physics_system);
        input_manager->shutdown();
        input_manager.reset();
        // ---------------------RESOURCE-------------------------
        ResourceManager::self().shutdown();
        // ---------------------RENDER-------------------------
        ui_system->shutdown();
        ui_system.reset();
        render_system->shutdown();
        render_system.reset();
        window_manager->shutdown();
        window_manager.reset();
        // ---------------------CORE-------------------------
        time_system.reset();
        reflection::TypeMetaRegister::meta_unregister();
        EventSystem::shutdown();
        return true;
    }

    void SystemContext::runtime_start() {
        WorldManager::self().runtime_start();
    }

    void SystemContext::tick_one_frame() {
        update_tick(time_system->delta_time());
        render_tick();
    }

    void SystemContext::runtime_finalize() {
        WorldManager::self().runtime_finalize();
    }

    void SystemContext::update_tick(const float delta_time) {
        for (auto& layer : layer_stack) {
            layer->on_update(delta_time);
        }

        input_manager->update();
        WorldManager::self().runtime_update(delta_time);
        physics_system->step(delta_time);
    }

    void SystemContext::render_tick() {
        render_system->prepare();
        ui_system->begin_render();

        for (auto& layer : layer_stack) {
            layer->on_ui_render();
        }
        ui_system->end_render();
        render_system->present();
    }
} // dodoe
