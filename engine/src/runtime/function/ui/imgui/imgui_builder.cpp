// do@Redlive

#ifdef DODOE_EDITOR

#include "imgui_builder.h"

#include "imgui_style.h"
#include "runtime/function/render/render_api.h"

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

        ApplyImGuiStyle(window->getNativeWindow());

        const auto api_type = RenderApi::apiType();
        if (api_type == RenderApiType::OpenGL) {
            ImGui_ImplGlfw_InitForOpenGL(window, false);
        }
        else if (api_type == RenderApiType::Vulkan) {
            ImGui_ImplGlfw_InitForVulkan(window, false);
        }
        else {
            ImGui_ImplGlfw_InitForOther(window, false);
        }
    }

    void ImGuiBuilder::PrepareImGui() {
        if (!ImGui::GetCurrentContext()) {
            return;
        }

        ImGui_ImplGlfw_NewFrame();
        if (RenderApi::apiType() == RenderApiType::OpenGL) {
            ImGui_ImplOpenGL3_NewFrame();
        }
        ImGui::NewFrame();
    }

    void ImGuiBuilder::CleanupImGui() {
        if (ImGui::GetCurrentContext()) {
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
        }
    }

} // dodoe

#endif//DODOE_EDITOR