// do@Redlive

#include "mono_system.h"

namespace dodoe {

    MonoSystem::~MonoSystem() = default;

    void MonoSystem::start(Registry& reg) {
        (void)reg;
        getMonoRuntime()->onRuntimeStart();
    }

    void MonoSystem::update(Registry& reg, float dt) {
        (void)reg;
        (void)dt;
        getMonoRuntime()->onRuntimeUpdate();
    }

    void MonoSystem::finalize(Registry& reg) {
        (void)reg;
        getMonoRuntime()->onRuntimeFinalize();
    }

    ScriptRuntime* MonoSystem::getMonoRuntime() {
        return GetScriptSystem()->getMonoRuntime();
    }

} // dodoe
