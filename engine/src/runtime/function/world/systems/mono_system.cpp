// do@Redlive

#include "mono_system.h"

namespace dodoe {

    MonoSystem::~MonoSystem() = default;

    void MonoSystem::start(Registry& reg) {
        (void)reg;
        getScriptRuntime()->onRuntimeStart();
    }

    void MonoSystem::update(Registry& reg, float dt) {
        (void)reg;
        (void)dt;
        getScriptRuntime()->onRuntimeUpdate();
    }

    void MonoSystem::finalize(Registry& reg) {
        (void)reg;
        getScriptRuntime()->onRuntimeFinalize();
    }

    ScriptRuntime* MonoSystem::getScriptRuntime() {
        return GetScriptSystem()->getScriptRuntime();
    }

} // dodoe
