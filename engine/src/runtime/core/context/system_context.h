//
// Created by GreenMuffin on 2025/11/1.
//

#ifndef DODOE_SYSTEM_CONTEXT_H
#define DODOE_SYSTEM_CONTEXT_H

#include "dopch.h"

#include "runtime/core/application.h"
#include "runtime/core/layer/layer_stack.h"

namespace dodoe {

    class WindowManager;
    class RenderSystem;
    class UiSystem;
    class TimeSystem;
    class InputManager;
    class ScriptSystem;
    class PhysicsSystem;

    class World;

    struct SystemContextCreateInfo {
        ApplicationSpecification spec{};
    };
 
    class SystemContext {
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

        static Scope<SystemContext> create(SystemContextCreateInfo create_info);
        static void destroy(Scope<SystemContext>& context);
 
        void runtime_start();
        void tick_one_frame();
        void runtime_finalize();        
    private:
        [[nodiscard]] bool initialize_systems(SystemContextCreateInfo create_info);
        [[nodiscard]] bool shutdown_systems();
        
        void update_tick(float dt);
        void render_tick();
    };

} // dodoe

#endif //DODOE_SYSTEM_CONTEXT_H
