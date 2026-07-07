#include "dopch.h"

#include "system_context.h"
#include "runtime/resource/resource_manager.h"

#include "runtime/core/event/event.h"
#include "runtime/core/event/event_system.h"
#include "runtime/core/meta/reflection/reflection_register.h"

#include "runtime/core/debug/debugger.h"
#ifdef DODOE_DEBUG
#include "runtime/service/debug/debug_imgui.h"
#endif
#include "runtime/core/layer/layer.h"
#include "runtime/core/layer/layer_stack.h"
#include "runtime/core/project/project.h"

#include "runtime/function/time/time_system.h"

#include "runtime/resource/resource_manager.h"
#include "runtime/resource/file/file_system.h"

#include "runtime/function/window/window.h"
#include "runtime/function/window/window_manager.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/render_settings.h"
#include "runtime/function/ui/ui_system.h"

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
        render_settings_init_info.threading_mode = m_init_info.spec.render_settings.threading_mode;
        DO_ASSERT(RenderSettings::Initialize(render_settings_init_info), "RenderSettings init failed");

        m_ui_system     = UISystem::Create({m_window_manager.get()});
        m_debugger      = Debugger::Create({});
#ifdef DODOE_DEBUG
        DebugImGui::RegisterDebugPanel();
#endif
        m_render_system = RenderSystem::Create({m_window_manager.get()});
        DO_ASSERT(m_render_system, "RenderSystem init failed");

        m_input_manager = InputManager::Create({});

        m_script_system = ScriptSystem::Create({});
        DO_ASSERT(m_script_system, "ScriptSystem init failed");
        (void)m_script_system->reloadScripts();

        m_physics_system = PhysicsSystem::Create({});
        DO_ASSERT(m_physics_system, "PhysicsSystem init failed");

        m_world = World::Create({"Main"});
        DO_ASSERT(m_world, "World init failed");

        auto threading_mode = m_init_info.spec.render_settings.threading_mode;
        auto* gfx = m_render_system->getGfx();
        auto device = GDrawCommandList.getDevice();

        GDrawCommandList.setDevice(device);

        switch (threading_mode) {
        case ThreadingMode::TripleThread:
            m_draw_thread = create_scope<DrawThread>();
            m_draw_thread->start(device, gfx);

            m_render_thread = create_scope<RenderThread>();
            m_render_thread->start(m_render_system.get(), m_draw_thread.get());
            m_render_system->setRenderThread(m_render_thread.get());
            break;

        case ThreadingMode::DualThread:
            m_render_thread = create_scope<RenderThread>();
            m_render_thread->start(m_render_system.get(), device, gfx);
            m_render_system->setRenderThread(m_render_thread.get());
            break;

        case ThreadingMode::SingleThread:
            m_render_thread = create_scope<RenderThread>();
            m_render_thread->setupForDirect(m_render_system.get(), device, gfx);
            m_render_system->setRenderThread(m_render_thread.get());
            break;
        }

        return true;
    }

    void SystemContext::startRuntime() {
        auto future = ResourceManager::Self().loadAssetsAsync();
        future.wait();

        DO_ASSERT(m_world->activateStartScene(), "World activate start scene failed");
        m_world->start();
    }

    void SystemContext::stopRuntime() {
    }

    void SystemContext::finalizeModules() {
        m_world->finalize();

        World::Destroy(m_world);
        ScriptSystem::Destroy(m_script_system);
        PhysicsSystem::Destroy(m_physics_system);

        m_layer_stack.clearLayers();

        m_render_thread->stop();
        m_render_thread.reset();

        m_draw_thread.reset();

        InputManager::Destroy(m_input_manager);

        ResourceManager::Self().shutdown();
        RenderSystem::Destroy(m_render_system);
        UISystem::Destroy(m_ui_system);
#ifdef DODOE_DEBUG
        DebugImGui::UnregisterDebugPanel();
#endif
        Debugger::Destroy(m_debugger);
        WindowManager::Destroy(m_window_manager);

        TimeSystem::Destroy(m_time_system);
    }

    void SystemContext::postShutdown() {
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
        if (m_debugger) { m_debugger->onRender(); }
        for (auto& layer : m_layer_stack) { layer->renderTick(); }

        if (m_render_thread->getMode() == ThreadingMode::SingleThread) {
            m_render_thread->executeFrameOnce();
        } else {
            m_render_thread->submitAndWait();
        }
    }

} // dodoe
