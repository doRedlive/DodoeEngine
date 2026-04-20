// do@GreenMuffin

#pragma once

#include "dopch.h"

#include "system.h"
#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/script/script_system.h"
#include "runtime/function/script/script_runtime.h"

namespace dodoe {

    class MonoSystem : public System {
    public: 
        ~MonoSystem() override = default;

        void start(Registry& reg) override {
            getMonoRuntime()->onRuntimeStart();
        }

        void update(Registry& reg, const float dt) override {
            (void)dt;
            getMonoRuntime()->onRuntimeUpdate();
        }

        void finalize(Registry& reg) override {
            getMonoRuntime()->onRuntimeFinalize();
        }
    private:
        ScriptRuntime* getMonoRuntime() {
            return Application::self().context().script_system->getMonoRuntime();
        }
    };

} // dodoe