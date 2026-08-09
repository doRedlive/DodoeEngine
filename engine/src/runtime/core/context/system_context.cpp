#include "dopch.h"

#include "system_context.h"
#include "runtime/core/memory/memory.h"
#include "runtime/resource/resource_manager.h"

#include "runtime/core/event/event.h"
#include "runtime/core/event/event_system.h"
#include "runtime/core/meta/reflection/reflection_register.h"

#include "runtime/core/debug/debugger.h"
#ifdef DODOE_DEBUG_ENABLED
#include "runtime/service/debug/debug_imgui.h"
#include "runtime/function/ui/imgui/imgui_builder.h"
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
        Memory::Init();
        EventSystem::Initialize();
        TypeMetaRegister::MetaRegister();
        return true;
    }

    Bool SystemContext::initializeModules() {
        m_service_manager = ServiceManager::Create({});

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
        render_settings_init_info.present_mode = m_init_info.spec.render_settings.present_mode;
        DO_ASSERT(RenderSettings::Initialize(render_settings_init_info), "RenderSettings init failed");

#ifdef DODOE_DEBUG_ENABLED
        ImGuiBuilder::SetupImGui(m_window_manager->getWindow()->getNativeWindow());
#endif

        m_ui_manager = UIManager::Create({m_window_manager.get()});

        m_debugger      = Debugger::Create({});
#ifdef DODOE_DEBUG_ENABLED
        DebugImGui::RegisterDebugPanel();
#endif
        m_render_system = RenderSystem::Create({m_window_manager.get()});
        DO_ASSERT(m_render_system, "RenderSystem init failed");

        m_input_manager = InputManager::Create({});

        m_script_system = ScriptSystem::Create({});
        DO_ASSERT(m_script_system, "ScriptSystem init failed");
        m_physics_system = PhysicsSystem::Create({});
        DO_ASSERT(m_physics_system, "PhysicsSystem init failed");

        m_world = World::Create({"Main"});
        DO_ASSERT(m_world, "World init failed");

        const auto threading_mode = RenderSettings::GetThreadingMode();
        auto* gfx = m_render_system->getGfx();
        auto device = gfx->getDevice();

        switch (threading_mode) {
        case ThreadingMode::TripleThread:
            m_draw_thread = create_scope<DrawThread>();
            m_draw_thread->start(device, gfx);

            m_render_thread = create_scope<RenderThread>(RenderFrameTask([this] {
                m_render_system->renderFrame(ThreadingMode::TripleThread, m_draw_thread.get());
            }));
            m_render_thread->start(threading_mode);
            m_render_system->setRenderThread(m_render_thread.get());
            break;

        case ThreadingMode::DualThread:
            m_render_system->releaseApplicationGraphicsContext();
            m_render_thread = create_scope<RenderThread>(RenderFrameTask([this] {
                m_render_system->renderFrameOnRenderThread(ThreadingMode::DualThread, nullptr);
            }), RenderFrameTask([this] {
                m_render_system->releaseApplicationGraphicsContext();
            }));
            m_render_thread->start(threading_mode);
            m_render_system->setRenderThread(m_render_thread.get());
            break;

        case ThreadingMode::SingleThread:
            m_render_thread = create_scope<RenderThread>(RenderFrameTask([this] {
                m_render_system->renderFrame(ThreadingMode::SingleThread, nullptr);
            }));
            m_render_system->setRenderThread(m_render_thread.get());
            break;
        }

        return true;
    }

    void SystemContext::startRuntime() {
        if (RenderSettings::GetThreadingMode() == ThreadingMode::DualThread) {
            DO_ASSERT(m_render_system->acquireApplicationGraphicsContext(),
                "SystemContext failed to acquire graphics context for startup.");
        }

        DO_ASSERT(ResourceManager::Self().loadAssets(), "Failed to load asset database");

        const auto active_project = Project::ActiveProject();
        DO_ASSERT(active_project, "No active project when starting runtime");
        const auto& start_scene_name = active_project->config().start_scene_name;
        DO_ASSERT(!start_scene_name.empty(), "StartSceneName is empty!");

        Scene* start_scene = m_world->loadScene(start_scene_name, LoadSceneMode::Single);
        DO_ASSERT(start_scene, "World failed to load start scene '{}'", start_scene_name);

        m_world->start();
    }

    void SystemContext::stopRuntime() {
    }

    void SystemContext::finalizeModules() {
        if (RenderSettings::GetThreadingMode() == ThreadingMode::DualThread) {
            DO_ASSERT(m_render_system->acquireApplicationGraphicsContext(),
                "SystemContext failed to acquire graphics context for shutdown.");
        }

        m_world->finalize();

        World::Destroy(m_world);
        ScriptSystem::Destroy(m_script_system);
        PhysicsSystem::Destroy(m_physics_system);

        m_layer_stack.clearLayers();

        m_render_thread->stop();
        if (!m_render_system->acquireApplicationGraphicsContext()) {
            DO_ERROR("SystemContext failed to acquire graphics context during shutdown.");
        }
        m_render_thread.reset();

        m_draw_thread.reset();

        InputManager::Destroy(m_input_manager);

        ResourceManager::Self().shutdown();
        RenderSystem::Destroy(m_render_system);
        ServiceManager::Destroy(m_service_manager);
        UIManager::Destroy(m_ui_manager);
#ifdef DODOE_DEBUG_ENABLED
        ImGuiBuilder::CleanupImGui();
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
        if (RenderSettings::GetThreadingMode() == ThreadingMode::DualThread) {
            DO_ASSERT(m_render_system->acquireApplicationGraphicsContext(),
                "SystemContext failed to acquire graphics context for update.");
        }

        for (auto& layer : m_layer_stack) {
            layer->updateTick(delta_time);
        }

        m_input_manager->update();
        m_world->update(delta_time);
        m_physics_system->step(delta_time);
        m_ui_manager->update(delta_time);
    }

    void SystemContext::renderTick() {
#ifdef DODOE_DEBUG_ENABLED
        ImGuiBuilder::PrepareImGui();
#endif
        if (m_debugger) { m_debugger->onRender(); }
        for (auto& layer : m_layer_stack) { layer->renderTick(); }
#ifdef DODOE_DEBUG_ENABLED
        ImGuiBuilder::RenderImGui();
#endif

        if (m_render_thread->getMode() == ThreadingMode::DualThread) {
            m_render_system->releaseApplicationGraphicsContext();
        }

        switch (m_render_thread->getMode()) {
        case ThreadingMode::SingleThread:
            m_render_thread->executeFrameOnce();
            break;
        case ThreadingMode::DualThread:
            m_render_thread->submitAndWait();
            break;
        case ThreadingMode::TripleThread:
            m_render_thread->submit();
            break;
        }
    }

} // dodoe
