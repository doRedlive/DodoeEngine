// do@Redlive

#include "debugger.h"

#ifdef DODOE_DEBUG
#include "imgui/imgui.h"
#include "runtime/function/ui/imgui/imgui_builder.h"
#endif

namespace dodoe {

    bool Debugger::initialize(const DebuggerCreateInfo& info) {
        (void)info;
        return true;
    }

    void Debugger::shutdown() {
    }

    void Debugger::onRender() {
#ifdef DODOE_DEBUG
        if (!ImGuiBuilder::GetContext()) return;
        ImGuiContext* prev_ctx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(ImGuiBuilder::GetContext());
        onImGuiRendr();
        ImGui::SetCurrentContext(prev_ctx);
#endif
    }

    void Debugger::onImGuiRendr() {
#ifdef DODOE_DEBUG
        ImGui::ShowDemoWindow();

        ImGui::Begin("Dodoe Debugger");

        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("FPS: %.1f (%.3f ms)", io.Framerate, 1000.0f / io.Framerate);
        ImGui::Text("Display: %.0f x %.0f", io.DisplaySize.x, io.DisplaySize.y);
        ImGui::Text("Mouse: (%.0f, %.0f)", io.MousePos.x, io.MousePos.y);
        ImGui::Text("WantCaptureMouse: %s", io.WantCaptureMouse ? "true" : "false");
        ImGui::Text("WantCaptureKeyboard: %s", io.WantCaptureKeyboard ? "true" : "false");

        ImGui::Separator();

        static float f_val = 0.5f;
        ImGui::SliderFloat("float", &f_val, 0.0f, 1.0f);

        static int i_val = 0;
        ImGui::SliderInt("int", &i_val, 0, 100);

        static bool b_val = true;
        ImGui::Checkbox("checkbox", &b_val);

        static float color[3] = { 0.8f, 0.3f, 0.6f };
        ImGui::ColorEdit3("color", color);

        if (ImGui::Button("Click Me")) {
            i_val += 10;
        }
        ImGui::SameLine();
        ImGui::Text("counter: %d", i_val);

        ImGui::End();
#endif
    }

} // dodoe
