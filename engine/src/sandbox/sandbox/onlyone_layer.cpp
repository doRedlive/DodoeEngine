//
// Created by Redlive on 2026/3/23.
//

#include "onlyone_layer.h"

#include "runtime/core/application.h"
#include "runtime/core/system_context.h"
#include "runtime/function/script/script_system.h"

namespace sandbox {

    void OnlyoneLayer::on_attach() {
        dodoe::Application::self().context().script_system->execute("engine/src/sandbox/proj/OnlyOne/Scripts/main.lua");
    }

    void OnlyoneLayer::on_detach() {

    }

    void OnlyoneLayer::on_update(const float dt) {

    }

    void OnlyoneLayer::on_ui_render() {

    }

} // sandbox