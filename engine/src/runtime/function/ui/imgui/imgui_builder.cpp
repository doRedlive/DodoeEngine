// do@Redlive

#ifdef DODOE_DEBUG_ENABLED

#include "imgui_builder.h"

#include "imgui_style.h"
#include "runtime/function/render/render_settings.h"
#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
namespace dodoe {

    void ImGuiBuilder::SetupImGui(GLFWwindow* window) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        s_context = ImGui::GetCurrentContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        ApplyImGuiStyle(window);

        // Build font atlas early to satisfy atlas->TexIsBuilt assertion
        // when running without a dedicated renderer backend
        unsigned char* font_pixels = nullptr;
        int font_width = 0, font_height = 0;
        io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);

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

    void ImGuiBuilder::RenderImGui() {
        if (!ImGui::GetCurrentContext()) {
            return;
        }
        ImGui::Render();
        SerializeImGuiDrawData(s_packet);
    }

    void ImGuiBuilder::SerializeImGuiDrawData(ImGuiRenderPacket& out_packet) {
        const auto* draw_data = ImGui::GetDrawData();
        out_packet.lists.clear();
        if (!draw_data || draw_data->CmdListsCount <= 0) {
            out_packet.display_pos = Vector2f(0.0f, 0.0f);
            out_packet.display_size = Vector2f(0.0f, 0.0f);
            return;
        }

        out_packet.display_pos = Vector2f(draw_data->DisplayPos.x, draw_data->DisplayPos.y);
        out_packet.display_size = Vector2f(draw_data->DisplaySize.x, draw_data->DisplaySize.y);

        out_packet.lists.resize(draw_data->CmdListsCount);
        for (int n = 0; n < draw_data->CmdListsCount; ++n) {
            const auto* draw_list = draw_data->CmdLists[n];
            ImGuiRenderList& dst = out_packet.lists[n];

            dst.vertices.clear();
            if (draw_list->VtxBuffer.Size > 0) {
                dst.vertices.assign(draw_list->VtxBuffer.Data,
                                    draw_list->VtxBuffer.Data + draw_list->VtxBuffer.Size);
            }
            dst.indices.clear();
            if (draw_list->IdxBuffer.Size > 0) {
                dst.indices.assign(draw_list->IdxBuffer.Data,
                                   draw_list->IdxBuffer.Data + draw_list->IdxBuffer.Size);
            }

            dst.commands.resize(draw_list->CmdBuffer.Size);
            for (int i = 0; i < draw_list->CmdBuffer.Size; ++i) {
                const auto& src = draw_list->CmdBuffer[i];
                ImGuiRenderCmd& cmd = dst.commands[i];
                cmd.clip_rect = Vector4f(src.ClipRect.x, src.ClipRect.y, src.ClipRect.z, src.ClipRect.w);
                cmd.texture_id = src.GetTexID();
                cmd.vtx_offset = src.VtxOffset;
                cmd.idx_offset = src.IdxOffset;
                cmd.elem_count = src.ElemCount;
                cmd.user_callback = src.UserCallback;
            }
        }
    }

    void ImGuiBuilder::CleanupImGui() {
        if (!ImGui::GetCurrentContext()) return;
        if (s_glfwBackendInit) {
            ImGui_ImplGlfw_Shutdown();
            s_glfwBackendInit = false;
        }
        ImGui::DestroyContext();
        s_context = nullptr;
    }

} // dodoe

#endif//DODOE_DEBUG_ENABLED
