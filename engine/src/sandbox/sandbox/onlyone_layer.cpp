//
// Created by Redlive on 2026/3/23.
//

#include "onlyone_layer.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/script/script_system.h"

namespace sandbox {

    void OnlyoneLayer::attach() {
        dodoe::Application::Self().context().script_system->executeLua("engine/src/sandbox/proj/OnlyOne/Scripts/main.lua");
    }

    void OnlyoneLayer::detach() {

    }

    void OnlyoneLayer::updateTick(const float dt) {

    }

    void OnlyoneLayer::renderTick() {

    }

} // sandbox