// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/application.h"
#include "runtime/core/layer/layer_stack.h"
#include "runtime/function/world/world.h"
#include "runtime/function/input/input_manager.h"
#include "runtime/function/physics/physics_system.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/script/script_system.h"
#include "runtime/function/time/time_system.h"
#include "runtime/function/ui/ui_system.h"
#include "runtime/function/window/window_manager.h"

namespace dodoe {
    struct SystemContextCreateInfo {
        ApplicationSpecification spec{};
    };

    class SystemContext : public Managed<SystemContext, SystemContextCreateInfo> {
        friend class Managed<SystemContext, SystemContextCreateInfo>;
    public:
        ~SystemContext();
        
        Scope<WindowManager> window_manager {nullptr};
        Scope<PhysicsSystem> physics_system {nullptr};
        Scope<RenderSystem>  render_system  {nullptr};
        Scope<InputManager>  input_manager  {nullptr};
        Scope<ScriptSystem>  script_system  {nullptr};
        Scope<TimeSystem>    time_system    {nullptr};
        Scope<UiSystem>      ui_system      {nullptr};
        Scope<World>         world          {nullptr};
        LayerStack layer_stack{};
        bool m_runtime_started{false};


        void startRuntime();
        void tickOneFrame();
        void finalizeRuntime();        
    private:
        [[nodiscard]] bool initialize(SystemContextCreateInfo create_info);
        void shutdown();

        [[nodiscard]] bool initializeSystems(SystemContextCreateInfo create_info);
        [[nodiscard]] bool shutdownSystems();
        
        void updateTick(float dt);
        void renderTick();
    };

} // dodoe
