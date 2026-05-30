// do@Redlive

#include "dopch.h"

#include "ui_system.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/core/layer/layer_stack.h"

#ifdef DODOE_EDITOR
#include "runtime/function/ui/imgui/imgui_builder.h"
#endif

namespace dodoe {

    bool UISystem::initialize(const UISystemCreateInfo& info) {
        auto* window = info.window_manager->getWindow();

#ifdef DODOE_EDITOR
        ImGuiBuilder::SetupImGui(window->getNativeWindow());
#endif
        return true;
    }

    void UISystem::prepare() {
#ifdef DODOE_EDITOR
        ImGuiBuilder::PrepareImGui();
#endif
    }

    void UISystem::shutdown() {
#ifdef DODOE_EDITOR
        ImGuiBuilder::CleanupImGui();
#endif
    }

} // dodoe
