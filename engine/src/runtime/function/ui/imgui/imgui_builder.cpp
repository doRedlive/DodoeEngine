// do@Redlive

#ifdef DODOE_EDITOR

#include "imgui_builder.h"

#include "imgui_style.h"
#include "runtime/function/render/render_settings.h"
#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

namespace dodoe {

    void ImGuiBuilder::SetupImGui(GLFWwindow* window) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        ApplyImGuiStyle(window);

        if (!window) return;  // Host mode: no GLFW window, skip backend init

        const auto api_type = RenderSettings::GetRenderBackendApiType();
        if (api_type == RenderBackendApiType::OpenGL) {
            ImGui_ImplGlfw_InitForOpenGL(window, false);
        }
        else if (api_type == RenderBackendApiType::Vulkan) {
            ImGui_ImplGlfw_InitForVulkan(window, false);
        }
        else {
            ImGui_ImplGlfw_InitForOther(window, false);
        }
        s_glfwBackendInit = true;
    }

    void ImGuiBuilder::PrepareImGui() {
        if (!ImGui::GetCurrentContext()) {
            return;
        }

        if (s_glfwBackendInit) {
            ImGui_ImplGlfw_NewFrame();
            if (RenderSettings::GetRenderBackendApiType() == RenderBackendApiType::OpenGL) {
                ImGui_ImplOpenGL3_NewFrame();
            }
        } else {
            auto& io = ImGui::GetIO();
            auto* window = Application::Self().context().getWindowManager()->getWindow();
            if (window) {
                io.DisplaySize = ImVec2(static_cast<float>(window->getWidth()),
                                        static_cast<float>(window->getHeight()));
                io.DeltaTime = Application::Self().context().getTimeSystem()->getDeltaTime();
            }
        }
        ImGui::NewFrame();
    }

    void ImGuiBuilder::CleanupImGui() {
        if (!ImGui::GetCurrentContext()) return;
        if (s_glfwBackendInit) {
            ImGui_ImplGlfw_Shutdown();
            s_glfwBackendInit = false;
        }
        ImGui::DestroyContext();
    }

} // dodoe

#endif//DODOE_EDITOR
