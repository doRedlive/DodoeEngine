//
// Created by GreenMuffin on 2025/11/08.
//

#include "dopch.h"

#include "ui_system.h"

#include "runtime/function/window/window.h"
#include "runtime/function/window/window_manager.h"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

namespace dodoe {

    void UiSystem::initialize(WindowManager* window_manager) {
        DoAssert(window_manager, "UiSystem::initialize: window_manager is null.");
        auto* window = window_manager->active_window();
        DoAssert(window, "UiSystem::initialize: active_window is null.");

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui_ImplGlfw_InitForOpenGL(window->native_window(), true);
        ImGui_ImplOpenGL3_Init("#version 450");
        DoInfo("Ui system initialize success.");
    }

    void UiSystem::begin_render() {
        ImGui_ImplGlfw_NewFrame();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();
    }

    void UiSystem::end_render() {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void UiSystem::shutdown() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        DoInfo("Ui system shutdown success.");
    }
};
