// do@Redlive

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
        m_init_info = create_info;
        Bool success = preInit();
        return success;
    }

    void SystemContext::shutdown() {

    }

    Bool SystemContext::preInit() {
        Log::Initialize();
        EventSystem::Initialize();
        TypeMetaRegister::MetaRegister();
        return true;
    }

    Bool SystemContext::initializeModules() {
        m_time_system = TimeSystem::Create({});
        DO_ASSERT(m_time_system, "TimeSystem init failed!");

        ResourceManager::Self().initialize({});

        WindowManagerCreateInfo window_manager_create_info;
        window_manager_create_info.host_handle = m_init_info.spec.host_handle;
        window_manager_create_info.prop.title = m_init_info.spec.name.c_str();
        window_manager_create_info.prop.width = m_init_info.spec.width;
        window_manager_create_info.prop.height = m_init_info.spec.height;
        window_manager_create_info.prop.backend_api = m_init_info.spec.render_settings.api;
        m_window_manager = WindowManager::Create(window_manager_create_info);

        RenderSettingsInitInfo render_settings_init_info;
        render_settings_init_info.api      = m_init_info.spec.render_settings.api;
        render_settings_init_info.pipeline = m_init_info.spec.render_settings.pipeline;
        DO_ASSERT(RenderSettings::Initialize(render_settings_init_info), "RenderSettings init failed");

        m_ui_system     = UISystem::Create({m_window_manager.get()});
        m_render_system = RenderSystem::Create({m_window_manager.get()});
        DO_ASSERT(m_render_system, "RenderSystem init failed");

        m_input_manager = InputManager::Create({m_render_system->getViewportManager()});

        m_script_system = ScriptSystem::Create({});
        DO_ASSERT(m_script_system, "ScriptSystem init failed");
        (void)m_script_system->reloadScripts();

        m_physics_system = PhysicsSystem::Create({});
        DO_ASSERT(m_physics_system, "PhysicsSystem init failed");

        m_world = World::Create({"Main"});
        DO_ASSERT(m_world, "World init failed");

        m_render_thread = create_scope<RenderThread>();
        m_render_thread->start([this] { tickRenderingTask(); });
    }

    void SystemContext::startRuntime() {
        ResourceManager::Self().loadAssetsAsync();

        // need to wait load assets;
        // Project::Load();
        DO_ASSERT(m_world->activateStartScene(), "World activate start scene failed");
        m_world->start();
    }

    void SystemContext::finalizeModules() {

        m_world->finalize();

        World::Destroy(m_world);
        ScriptSystem::Destroy(m_script_system);
        PhysicsSystem::Destroy(m_physics_system);

        m_layer_stack.clearLayers();

        m_render_thread->stop();
        m_render_thread.reset();
        InputManager::Destroy(m_input_manager);

        ResourceManager::Self().shutdown();
        RenderSystem::Destroy(m_render_system);
        UISystem::Destroy(m_ui_system);
        WindowManager::Destroy(m_window_manager);

        TimeSystem::Destroy(m_time_system);
        TypeMetaRegister::MetaUnregister();
        EventSystem::Shutdown();
    }

    void SystemContext::tickOneFrame() {
        updateTick(m_time_system->getDeltaTime());
        renderTick();
    }

    void SystemContext::updateTick(const float delta_time) {
        for (auto& layer : m_layer_stack) {
            layer->updateTick(delta_time);
        }

        m_input_manager->update();
        if (m_world) {
            m_world->update(delta_time);
        }
        if (m_physics_system) {
            m_physics_system->step(delta_time);
        }
    }

    void SystemContext::renderTick() {
        if (m_ui_system) { m_ui_system->prepare(); }
        for (auto& layer : m_layer_stack) { layer->renderTick(); }
        if (m_render_thread) { m_render_thread->submitAndWait(); }
    }

    void SystemContext::tickRenderingTask() {
        if (m_render_system) {
            m_render_system->prepare();
            m_render_system->present();
        }
    }

} // dodoe
