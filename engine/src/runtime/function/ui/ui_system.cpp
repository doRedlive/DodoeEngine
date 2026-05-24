//
// Created by GreenMuffin on 2025/11/08.
//

#include "dopch.h"

#include "ui_system.h"

#include "runtime/function/window/window_manager.h"

#ifdef DODOE_EDITOR
#include "runtime/function/render/render_api.h"
#include "editor/cakery/style/imgui_style.h"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#endif

namespace dodoe {

    void UiSystem::initialize(WindowManager* window_manager) {
#ifdef DODOE_EDITOR
        auto* window = window_manager->getWindow();

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        const auto api_type = RenderApi::apiType();
        if (api_type == RenderApiType::OpenGL) {
            ImGui_ImplGlfw_InitForOpenGL(window->getNativeWindow(), false);
        }
        else if (api_type == RenderApiType::Vulkan) {
            ImGui_ImplGlfw_InitForVulkan(window->getNativeWindow(), false);
        }
        else {
            ImGui_ImplGlfw_InitForOther(window->getNativeWindow(), false);
        }

        ApplyImGuiStyle(window->getNativeWindow());
#else
        (void)window_manager;
#endif
    }

    void UiSystem::prepare() {
#ifdef DODOE_EDITOR
        if (!ImGui::GetCurrentContext()) {
            return;
        }

        ImGui_ImplGlfw_NewFrame();
        if (RenderApi::apiType() == RenderApiType::OpenGL) {
            ImGui_ImplOpenGL3_NewFrame();
        }
        ImGui::NewFrame();
#endif
    }

    void UiSystem::shutdown() {
#ifdef DODOE_EDITOR
        if (ImGui::GetCurrentContext()) {
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
        }
#endif
    }
};
