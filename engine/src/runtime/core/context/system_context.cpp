// Created by GreenMuffin on 2025/11/1.

#include "dopch.h"

#include "system_context.h"
#include "runtime/resource/resource_manager.h"
// core
#include "runtime/core/event/event.h"
#include "runtime/core/event/event_system.h"
#include "runtime/core/meta/reflection/reflection_register.h"

#include "runtime/core/layer/layer.h"
#include "runtime/core/layer/layer_stack.h"
#include "runtime/core/project/project.h"

#include "runtime/function/time/time_system.h"

// resource
#include "runtime/resource/resource_manager.h"
#include "runtime/resource/file/file_system.h"

// render
#include "runtime/function/window/window.h"
#include "runtime/function/window/window_manager.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/render_settings.h"
#include "runtime/function/ui/ui_system.h"

// game
#include "runtime/function/world/world.h"
#include "runtime/function/input/input_manager.h"
#include "runtime/function/script/script_system.h"
#include "runtime/function/physics/physics_system.h"

namespace dodoe {

    SystemContext::~SystemContext() = default;

    bool SystemContext::initialize(SystemContextCreateInfo create_info) {
        return initializeSystems(std::move(create_info));
    }

    void SystemContext::shutdown() {
        DO_ASSERT(shutdownSystems(), "System shutdown failed!");
    }

    bool SystemContext::initializeSystems(SystemContextCreateInfo create_info) {
        window_manager = create_scope<WindowManager>();
        input_manager  = create_scope<InputManager>();
        time_system    = create_scope<TimeSystem>();
        // ---------------------CORE-------------------------
        Log::Initialize();
        EventSystem::Initialize();
        TypeMetaRegister::meta_register();
        // ---------------------RESOURCE-------------------------
        ResourceManager::Self().initialize();
        // ---------------------RENDER-------------------------
        window_manager->initialize({create_info.spec});
        DO_ASSERT(RenderSettings::Initialize(create_info.spec.render_settings), "RenderSettings initialize failed!");
        ui_system = UISystem::Create({window_manager.get()});
        render_system = RenderSystem::Create({window_manager.get()});
        DO_ASSERT(render_system, "RenderSystem initialize failed!");

        input_manager->initialize({render_system->getViewportManager()});

        m_render_thread = create_scope<RenderThread>();
        m_render_thread->start([this] { tickRenderingTask(); });

        return true;
    }

    bool SystemContext::shutdownSystems() {
        layer_stack.clearLayers();

        m_render_thread->stop();
        m_render_thread.reset();

        // ---------------------GAME-------------------------
        input_manager->shutdown();
        input_manager.reset();
        // ---------------------RESOURCE-------------------------
        ResourceManager::Self().shutdown();
        // ---------------------RENDER-------------------------
        RenderSystem::Destroy(render_system);
        UISystem::Destroy(ui_system);
        window_manager->shutdown();
        window_manager.reset();
        // ---------------------CORE-------------------------
        time_system.reset();
        TypeMetaRegister::meta_unregister();
        EventSystem::Shutdown();
        return true;
    }

    void SystemContext::startRuntime() {
        ResourceManager::Self().loadAssets();

        script_system = ScriptSystem::Create({});
        DO_ASSERT(script_system, "ScriptSystem initialize failed!");
        (void)script_system->reloadScripts();

        physics_system = PhysicsSystem::Create({});
        DO_ASSERT(physics_system, "PhysicsSystem initialize failed!");

        world = World::Create({"Main"});
        DO_ASSERT(world, "World initialize failed!");

        DO_ASSERT(world->activateStartScene(), "World active start scene failed!");

        world->start();
        m_runtime_started = true;
    }

    void SystemContext::tickOneFrame() {
        updateTick(time_system->getDeltaTime());
        renderTick();
    }

    void SystemContext::finalizeRuntime() {
        world->finalize();

        World::Destroy(world);
        ScriptSystem::Destroy(script_system);
        PhysicsSystem::Destroy(physics_system);
    }

    void SystemContext::updateTick(const float delta_time) {
        for (auto& layer : layer_stack) {
            layer->updateTick(delta_time);
        }

        input_manager->update();
        if (world) {
            world->update(delta_time);
        }
        if (physics_system) {
            physics_system->step(delta_time);
        }
    }

    void SystemContext::renderTick() {
        ui_system->prepare();

        for (auto& layer : layer_stack) {
            layer->renderTick();
        }

        m_render_thread->submitAndWait();
    }

    void SystemContext::tickRenderingTask() {
        render_system->prepare();
        render_system->present();
    }

} // dodoe
