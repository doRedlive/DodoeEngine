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
#include "runtime/function/render/render_api.h"
#include "runtime/function/ui/ui_system.h"

// game
#include "runtime/function/world/world.h"
#include "runtime/function/input/input_manager.h"
#include "runtime/function/script/script_system.h"
#include "runtime/function/script/lua/lua_script_runtime.h"
#include "runtime/function/physics/physics_system.h"

namespace dodoe {

    SystemContext::~SystemContext() = default;

    Scope<SystemContext> SystemContext::create(SystemContextCreateInfo create_info) {
        auto context = create_scope<SystemContext>();
        DO_ASSERT(context->initialize_systems(create_info), "System initialize failed!");
        return context;
    }

    void SystemContext::destroy(Scope<SystemContext>& context) {
        if (!context) return;
        DO_ASSERT(context->shutdown_systems(), "System shutdown failed!");
        context.reset();
    }

    bool SystemContext::initialize_systems(SystemContextCreateInfo create_info) {
        window_manager = create_scope<WindowManager>();
        input_manager  = create_scope<InputManager>();
        time_system    = create_scope<TimeSystem>();
        ui_system      = create_scope<UiSystem>();
        // ---------------------CORE-------------------------
        Log::initialize();
        EventSystem::initialize();
        TypeMetaRegister::meta_register();
        // ---------------------RESOURCE-------------------------
        ResourceManager::self().initialize();
        // ---------------------RENDER-------------------------
        window_manager->initialize({create_info.spec});
        RenderApi::initialize({create_info.spec.render_api_type});
        ui_system->initialize(window_manager.get());
        render_system = RenderSystem::create({window_manager.get(), ui_system.get(), create_info.spec.render_api_type});
        DO_ASSERT(render_system, "RenderSystem initialize failed!");
        
        // ---------------------GAME-------------------------
        world = World::create({"Main"});

        input_manager->initialize({render_system->viewportManager()});
        physics_system = PhysicsSystem::create({});
        script_system  = ScriptSystem::create({});
 
        return true;
    }

    bool SystemContext::shutdown_systems() {
        // Destroy layers while EventSystem/World/Render are still valid.
        layer_stack.detach();
        layer_stack.clear_layers();

        // ---------------------GAME-------------------------
        World::destroy(world);
        ScriptSystem::destroy(script_system);
        PhysicsSystem::destroy(physics_system);
        input_manager->shutdown();
        input_manager.reset();
        // ---------------------RESOURCE-------------------------
        ResourceManager::self().shutdown();
        // ---------------------RENDER-------------------------
        RenderSystem::destroy(render_system);
        ui_system->shutdown();
        ui_system.reset();
        window_manager->shutdown();
        window_manager.reset();
        // ---------------------CORE-------------------------
        time_system.reset();
        TypeMetaRegister::meta_unregister();
        EventSystem::shutdown();
        return true;
    }

    void SystemContext::runtime_start() {
        world->runtime_start();
    }

    void SystemContext::tick_one_frame() {
        update_tick(time_system->delta_time());
        render_tick();
    }

    void SystemContext::runtime_finalize() {
        world->runtime_finalize();
    }

    void SystemContext::update_tick(const float delta_time) {
        for (auto& layer : layer_stack) {
            layer->updateTick(delta_time);
        }

        input_manager->update();
        world->runtime_update(delta_time);
        physics_system->step(delta_time);
    }

    void SystemContext::render_tick() {
        render_system->prepare();
        ui_system->prepare();

        for (auto& layer : layer_stack) {
            layer->renderTick();
        }
        render_system->present();
    }
} // dodoe
