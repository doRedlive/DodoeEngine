// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/application.h"
#include "runtime/core/layer/layer_stack.h"
#include "runtime/core/thread/render_thread.h"
#include "runtime/core/thread/draw_thread.h"
#include "runtime/function/world/world.h"
#include "runtime/function/input/input_manager.h"
#include "runtime/function/physics/physics_system.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/script/script_system.h"
#include "runtime/function/time/time_system.h"
#include "runtime/function/ui/ui_system.h"
#include "runtime/function/window/window_manager.h"

namespace dodoe {
    struct DODOE_API SystemContextCreateInfo {
        ApplicationSpecification spec{};
    };

    class DODOE_API SystemContext : public Managed<SystemContext, SystemContextCreateInfo> {
        friend class Managed<SystemContext, SystemContextCreateInfo>;

        Scope<RenderThread> m_render_thread{nullptr};
        Scope<DrawThread> m_draw_thread{nullptr};

    public:
        ~SystemContext();

        Scope<WindowManager> window_manager {nullptr};
        Scope<PhysicsSystem> physics_system {nullptr};
        Scope<RenderSystem>  render_system  {nullptr};
        Scope<InputManager>  input_manager  {nullptr};
        Scope<ScriptSystem>  script_system  {nullptr};
        Scope<TimeSystem>    time_system    {nullptr};
        Scope<UISystem>      ui_system      {nullptr};
        Scope<World>         world          {nullptr};
        LayerStack layer_stack{};
        Bool m_runtime_started{false};

        [[nodiscard]] WindowManager* getWindowManager() const { return window_manager.get(); }
        [[nodiscard]] RenderSystem*  getRenderSystem() const { return render_system.get(); }
        [[nodiscard]] InputManager*  getInputManager() const { return input_manager.get(); }
        [[nodiscard]] const LayerStack& getLayerStack() const { return layer_stack; }

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
