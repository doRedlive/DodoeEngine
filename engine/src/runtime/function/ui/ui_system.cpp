//
// Created by GreenMuffin on 2025/11/08.
//

#include "dopch.h"

#include "ui_system.h"

#include "runtime/function/window/window.h"
#include "runtime/function/window/window_manager.h"
#include "runtime/function/render/render_api.h"

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
        context_created_ = true;

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        const auto api_type = RenderApi::api_type();
        if (api_type == RenderApiType::OpenGL) {
            glfw_backend_initialized_ = ImGui_ImplGlfw_InitForOpenGL(window->native_window(), true);
            opengl_renderer_initialized_ = ImGui_ImplOpenGL3_Init("#version 450");
        } else {
            // Vulkan/other backends: keep input integration but skip OpenGL renderer backend.
            glfw_backend_initialized_ = ImGui_ImplGlfw_InitForOther(window->native_window(), true);
            opengl_renderer_initialized_ = false;
        }

    }

    void UiSystem::begin_render() {
        if (!context_created_ || !opengl_renderer_initialized_) {
            return;
        }

        if (glfw_backend_initialized_) {
            ImGui_ImplGlfw_NewFrame();
        }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();
    }

    void UiSystem::end_render() {
        if (!context_created_ || !opengl_renderer_initialized_) {
            return;
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void UiSystem::shutdown() {
        if (opengl_renderer_initialized_) {
            ImGui_ImplOpenGL3_Shutdown();
            opengl_renderer_initialized_ = false;
        }

        if (glfw_backend_initialized_) {
            ImGui_ImplGlfw_Shutdown();
            glfw_backend_initialized_ = false;
        }

        if (context_created_) {
            ImGui::DestroyContext();
            context_created_ = false;
        }
    }
};
