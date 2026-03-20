//
// Created by GreenMuffin on 2026/1/23.
//

#include "console_panel.h"

#include "runtime/function/log/log_system.h"

#include "imgui/imgui.h"

namespace cakery {

    void ConsolePanel::on_ui_render() {
        ImGui::Begin("Console");

        for (const auto& log_msgs = dodoe::Log::get_all_client_logs(); const auto& [content, level] : log_msgs) {
            if (level == dodoe::LogLevel::Error || level == dodoe::LogLevel::Critical) {
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
                ImGui::Text(content.c_str());
                ImGui::PopStyleColor();
            } else if (level == dodoe::LogLevel::Warn) {
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
                ImGui::Text(content.c_str());
                ImGui::PopStyleColor();
            } else if (level == dodoe::LogLevel::Info || level == dodoe::LogLevel::Debug) {
                ImGui::Separator();
                ImGui::Text(content.c_str());
            }
        }

        ImGui::End();
    }

} // cakery