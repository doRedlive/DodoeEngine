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
#endif//DODOE_DEBUG_ENABLED
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
        DO_PROFILE_SCOPE_CATEGORY("SystemContext::initialize", "startup");
        m_init_info = create_info;
        Bool success = preInit();
        return success;
    }

    void SystemContext::shutdown() {

    }

    Bool SystemContext::preInit() {
        DO_PROFILE_SCOPE_CATEGORY("SystemContext::preInit", "startup");
        Log::Initialize();
        Memory::Init();
        EventSystem::Initialize();
        TypeMetaRegister::MetaRegister();
        DO_INFO("Core services initialized.");
        return true;
    }

    Bool SystemContext::initializeModules() {
        DO_PROFILE_SCOPE_CATEGORY("SystemContext::initializeModules", "startup");
        m_service_manager = ServiceManager::Create({});
        DO_INFO("ServiceManager initialized.");

        m_time_system = TimeSystem::Create({});
        DO_INFO("TimeSystem initialized.");

        ResourceManager::Self().initialize({});
        DO_INFO("ResourceManager initialized.");

        RenderSettingsInitInfo render_settings_init_info;
        render_settings_init_info.api      = m_init_info.spec.render_settings.api;
        render_settings_init_info.pipeline = m_init_info.spec.render_settings.pipeline;
        render_settings_init_info.threading_mode = m_init_info.spec.render_settings.threading_mode;
        render_settings_init_info.present_mode = m_init_info.spec.render_settings.present_mode;
        render_settings_init_info.windowless = m_init_info.spec.render_settings.windowless;
        DO_ASSERT(RenderSettings::Initialize(render_settings_init_info), "RenderSettings init failed");
        DO_INFO("RenderSettings initialized.");

        if (!RenderSettings::IsWindowless()) {
            WindowManagerCreateInfo window_manager_create_info;
            window_manager_create_info.host_handle = m_init_info.spec.host_handle;
            window_manager_create_info.prop.title = m_init_info.spec.name.c_str();
            window_manager_create_info.prop.width = m_init_info.spec.width;
            window_manager_create_info.prop.height = m_init_info.spec.height;
            window_manager_create_info.prop.backend_api = m_init_info.spec.render_settings.api;
            window_manager_create_info.host_pixel_size = Vector2i(
                static_cast<Int32>(m_init_info.spec.pixel_width),
                static_cast<Int32>(m_init_info.spec.pixel_height));
            m_window_manager = WindowManager::Create(window_manager_create_info);
            DO_INFO("WindowManager initialized.");

#ifdef DODOE_DEBUG_ENABLED
            ImGuiBuilder::SetupImGui(m_window_manager->getWindow()->getNativeWindow());
#endif//DODOE_DEBUG_ENABLED

            m_ui_manager = UIManager::Create({m_window_manager.get()});
            DO_INFO("UIManager initialized.");

            m_debugger      = Debugger::Create({});
            DO_INFO("Debugger initialized.");
#ifdef DODOE_DEBUG_ENABLED
            DebugImGui::RegisterDebugPanel();
#endif//DODOE_DEBUG_ENABLED
            m_render_system = RenderSystem::Create({m_window_manager.get()});
            DO_ASSERT(m_render_system, "RenderSystem init failed");
            DO_INFO("RenderSystem initialized.");

            InputManagerInitInfo input_init_info;
            input_init_info.native_window = m_window_manager->getWindow()->getNativeWindow();
            input_init_info.host_mode = m_window_manager->getWindow()->isHostMode();
            m_input_manager = InputManager::Create(input_init_info);
            DO_INFO("InputManager initialized.");
        }

        m_script_system = ScriptSystem::Create({});
        DO_ASSERT(m_script_system, "ScriptSystem init failed");
        DO_INFO("ScriptSystem initialized.");
        m_physics_system = PhysicsSystem::Create({});
        DO_ASSERT(m_physics_system, "PhysicsSystem init failed");
        DO_INFO("PhysicsSystem initialized.");

        m_world = World::Create({"Main"});
        DO_ASSERT(m_world, "World init failed");
        DO_INFO("World initialized.");

        return true;
    }

    void SystemContext::startRuntime() {
        DO_PROFILE_SCOPE_CATEGORY("SystemContext::startRuntime", "startup");
        DO_PROFILE_MARK("SystemContext::startRuntime.acquireGraphicsContext", "startup");
        if (m_render_system) {
            DO_ASSERT(m_render_system->beginMainThreadFrame(),
                "SystemContext failed to acquire graphics context for startup.");
        }

        DO_ASSERT(ResourceManager::Self().loadAssets(), "Failed to load asset database");
        DO_PROFILE_MARK("SystemContext::startRuntime.assetsReady", "startup");

        const auto active_project = Project::ActiveProject();
        DO_ASSERT(active_project, "No active project when starting runtime");
        const auto& start_scene_name = active_project->config().start_scene_name;
        DO_ASSERT(!start_scene_name.empty(), "StartSceneName is empty!");

        DO_PROFILE_MARK("SystemContext::startRuntime.loadScene", "startup");
        Scene* start_scene = m_world->loadScene(start_scene_name, LoadSceneMode::Single);
        DO_ASSERT(start_scene, "World failed to load start scene '{}'", start_scene_name);

        DO_PROFILE_MARK("SystemContext::startRuntime.startWorld", "startup");
        m_world->start();
        DO_INFO("Runtime startup completed with scene '{}'.", start_scene_name);
    }

    void SystemContext::stopRuntime() {
    }

    void SystemContext::finalizeModules() {
        DO_PROFILE_SCOPE_CATEGORY("SystemContext::finalizeModules", "shutdown");
        if (m_render_system) {
            DO_ASSERT(m_render_system->beginMainThreadFrame(),
                "SystemContext failed to acquire graphics context for shutdown.");
        }

        m_world->finalize();

        World::Destroy(m_world);
        ScriptSystem::Destroy(m_script_system);
        PhysicsSystem::Destroy(m_physics_system);

        m_layer_stack.clearLayers();

        InputManager::Destroy(m_input_manager);

#ifdef DODOE_DEBUG_ENABLED
        ImGuiBuilder::CleanupImGui();
#endif//DODOE_DEBUG_ENABLED
        ResourceManager::Self().shutdown();
        RenderSystem::Destroy(m_render_system);
        ServiceManager::Destroy(m_service_manager);
        UIManager::Destroy(m_ui_manager);
#ifdef DODOE_DEBUG_ENABLED
        DebugImGui::UnregisterDebugPanel();
#endif//DODOE_DEBUG_ENABLED
        Debugger::Destroy(m_debugger);
        WindowManager::Destroy(m_window_manager);

        TimeSystem::Destroy(m_time_system);
        DO_INFO("Module shutdown completed.");
    }

    void SystemContext::postShutdown() {
        TypeMetaRegister::MetaUnregister();
        EventSystem::Shutdown();
    }

    void SystemContext::tickOneFrame() {
        DO_PROFILE_SCOPE_CATEGORY("SystemContext::tickOneFrame", "frame");
        updateTick(m_time_system->getDeltaTime());
        renderTick();
    }

    void SystemContext::updateTick(const float delta_time) {
        DO_PROFILE_SCOPE_CATEGORY("SystemContext::updateTick", "frame");
        if (m_render_system) {
            DO_ASSERT(m_render_system->beginMainThreadFrame(),
                "SystemContext failed to acquire graphics context for update.");
        }

        for (auto& layer : m_layer_stack) {
            layer->updateTick(delta_time);
        }

        m_world->update(delta_time);
        if (m_ui_manager) { m_ui_manager->update(delta_time); }
    }

    void SystemContext::renderTick() {
        DO_PROFILE_SCOPE_CATEGORY("SystemContext::renderTick", "frame");
        if (!m_render_system) { return; }
#ifdef DODOE_DEBUG_ENABLED
        ImGuiBuilder::PrepareImGui();
#endif//DODOE_DEBUG_ENABLED
        if (m_debugger) { m_debugger->onRender(); }
        for (auto& layer : m_layer_stack) { layer->renderTick(); }
#ifdef DODOE_DEBUG_ENABLED
        ImGuiBuilder::RenderImGui();
#endif//DODOE_DEBUG_ENABLED

        m_render_system->submitFrame();
    }

} // dodoe
