//
// Created by GreenMuffin on 2025/12/7.
//

#include "hierarchy_panel.h"

#include "cakery_helper.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "command/command_handler.h"

#include "runtime/core/world/components.h"
#include "runtime/core/world/game_object.h"

#include "runtime/function/context.h"


namespace cakery {

    HierarchyPanel::HierarchyPanel() : handler_(SceneHierarchyHandler(*this)) {

    }

    void HierarchyPanel::set_context(dodoe::Scene* context) {
        context_ = context;
    }

    void HierarchyPanel::on_ui_render() {
        ImGui::Begin("Scene Hierarchy");

        if (context_) {
            for (const auto game_objects = context_->get_all_game_objects(); const auto& game_object: game_objects) {
                if (!game_object) continue;
                draw_game_object_node_(game_object);
            }

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered()) {
                g_cakery_helper.set_selected_game_object(nullptr);
                selected_game_object_ = nullptr;
            }

            if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
                if (ImGui::MenuItem("Create GameObject")) {
                    CommandHandler::push_command(dodoe::create_ref<SceneHierarchyHandler::CreateGameObjectCmd>(handler_));
                }
                ImGui::EndPopup();
            }
        }

        ImGui::End();
    }

    void HierarchyPanel::draw_game_object_node_(dodoe::GameObject* game_object) {
        const auto name = game_object->get_name();
        ImGuiTreeNodeFlags flags = (selected_game_object_ == game_object ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
        flags |= ImGuiTreeNodeFlags_SpanAvailWidth;

        const bool is_opened = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(game_object->entity_handle_)), flags,"%s", name.c_str());

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            selected_game_object_ = game_object;
            g_cakery_helper.set_selected_game_object(selected_game_object_);
            DoDebug("SceneHierarchy: selected '{}' ptr={}.", name, static_cast<void*>(game_object));
        }

        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Delete GameObject")) {
                CommandHandler::push_command(dodoe::create_ref<SceneHierarchyHandler::DeleteGameObjectCmd>(handler_, game_object));
                if (g_cakery_helper.get_selected_game_object() == game_object) {
                    g_cakery_helper.set_selected_game_object(nullptr);
                    selected_game_object_ = nullptr;
                }
            }
            if (ImGui::MenuItem("Create Child GameObject")) {
                CommandHandler::push_command(dodoe::create_ref<SceneHierarchyHandler::CreateChildGameObjectCmd>(handler_, game_object));
            }
            ImGui::EndPopup();
        }

        if (is_opened) {
            constexpr ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            for (const auto children = game_object->get_children(); const auto& child : children) {
                draw_game_object_node_(child);
            }
            ImGui::TreePop();
        }
    }

}
