//
// Created by GreenMuffin on 2025/11/08.
//

#include "dopch.h"

#include "ui_system.h"

#include "runtime/function/window/window.h"
#include "runtime/function/window/window_manager.h"
#include "runtime/function/render/render_api.h"
#include "editor/cakery/style/imgui_style.h"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

namespace dodoe {

    void UiSystem::initialize(WindowManager* window_manager) {
        auto* window = window_manager->window();

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        const auto api_type = RenderApi::apiType();
        if (api_type == RenderApiType::OpenGL) {
            ImGui_ImplGlfw_InitForOpenGL(window->nativeWindow(), true);
        } 
        else if (api_type == RenderApiType::Vulkan) {
            ImGui_ImplGlfw_InitForVulkan(window->nativeWindow(), true);
        }
        else {
            ImGui_ImplGlfw_InitForOther(window->nativeWindow(), true);
        }

		ApplyImGuiStyle(window->nativeWindow());
    }

    void UiSystem::prepare() {
        if (!ImGui::GetCurrentContext()) {
            return;
        }

        ImGui_ImplGlfw_NewFrame();
        if (RenderApi::apiType() == RenderApiType::OpenGL) {
            ImGui_ImplOpenGL3_NewFrame();
        }
        ImGui::NewFrame();
    }

    void UiSystem::shutdown() {
        if (ImGui::GetCurrentContext()) {
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
        }
    }
};
