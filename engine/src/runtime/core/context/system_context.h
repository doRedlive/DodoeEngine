// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/application.h"
#include "runtime/core/debug/debugger.h"
#include "runtime/core/layer/layer_stack.h"
#include "runtime/service/service_manager.h"
#include "runtime/function/world/world.h"
#include "runtime/function/input/input_manager.h"
#include "runtime/function/physics/physics_system.h"
#include "runtime/function/audio/audio_system.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/script/script_system.h"
#include "runtime/function/time/time_system.h"
#include "runtime/function/ui/ui_manager.h"
#include "runtime/function/window/window_manager.h"

namespace dodoe {
    struct DODOE_API SystemContextCreateInfo {
        ApplicationSpecification spec{};
    };

    class DODOE_API SystemContext : public Managed<SystemContext, SystemContextCreateInfo> {
        friend class Managed<SystemContext, SystemContextCreateInfo>;

        SystemContextCreateInfo m_init_info{};

        Scope<WindowManager> m_window_manager {nullptr};
        Scope<PhysicsSystem> m_physics_system {nullptr};
        Scope<RenderSystem>  m_render_system  {nullptr};
        Scope<InputManager>  m_input_manager  {nullptr};
        Scope<AudioSystem>   m_audio_system   {nullptr};
        Scope<ScriptSystem>  m_script_system  {nullptr};
        Scope<TimeSystem>    m_time_system    {nullptr};
        Scope<UIManager>     m_ui_manager     {nullptr};

        Scope<World>         m_world          {nullptr};
        Scope<Debugger>      m_debugger       {nullptr};
        Scope<ServiceManager> m_service_manager{nullptr};
        LayerStack m_layer_stack{};
    public:
        ~SystemContext();

        [[nodiscard]] WindowManager* getWindowManager() const { return m_window_manager.get(); }
        [[nodiscard]] RenderSystem*  getRenderSystem()  const { return m_render_system.get(); }
        [[nodiscard]] PhysicsSystem* getPhysicsSystem() const { return m_physics_system.get(); }
        [[nodiscard]] InputManager*  getInputManager()  const { return m_input_manager.get(); }
        [[nodiscard]] AudioSystem*   getAudioSystem()   const { return m_audio_system.get(); }
        [[nodiscard]] ScriptSystem*  getScriptSystem()  const { return m_script_system.get(); }
        [[nodiscard]] TimeSystem*    getTimeSystem()    const { return m_time_system.get(); }
        [[nodiscard]] UIManager*     getUIManager()     const { return m_ui_manager.get(); }
        [[nodiscard]] World*         getWorld()         const { return m_world.get(); }
        [[nodiscard]] Debugger*      getDebugger()      const { return m_debugger.get(); }
        [[nodiscard]] ServiceManager* getServiceManager() const { return m_service_manager.get(); }
        [[nodiscard]] LayerStack& getLayerStack() { return m_layer_stack; }
        [[nodiscard]] const LayerStack& getLayerStack() const { return m_layer_stack; }

        Bool preInit();
        Bool initializeModules();
        void startRuntime();

        void finalizeModules();
        void stopRuntime();
        void postShutdown();

        void tickOneFrame();

    private:
        [[nodiscard]] Bool initialize(SystemContextCreateInfo create_info);
        void shutdown();

        void updateTick(float dt);
        void renderTick();

    };

    inline RenderSystem*  GetRenderSystem()  { return Application::Self().context().getRenderSystem(); }
    inline WindowManager* GetWindowManager() { return Application::Self().context().getWindowManager(); }
    inline TimeSystem*    GetTimeSystem()    { return Application::Self().context().getTimeSystem(); }
    inline World*         GetWorld()         { return Application::Self().context().getWorld(); }
    inline ScriptSystem*  GetScriptSystem()  { return Application::Self().context().getScriptSystem(); }
    inline PhysicsSystem* GetPhysicsSystem() { return Application::Self().context().getPhysicsSystem(); }
    inline InputManager*  GetInputManager()  { return Application::Self().context().getInputManager(); }
    inline AudioSystem*   GetAudioSystem()   { return Application::Self().context().getAudioSystem(); }
    inline UIManager*     GetUIManager()     { return Application::Self().context().getUIManager(); }
    inline Debugger*      GetDebugger()      { return Application::Self().context().getDebugger(); }

} // dodoe
