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
        ~MonoSystem() override;

        [[nodiscard]] SystemAccess getAccess() const override;
        void start(Registry& reg) override;
        void update(Registry& reg, float dt) override;
        void finalize(Registry& reg) override;

    private:
        ScriptRuntime* getScriptRuntime();
    };

} // dodoe
