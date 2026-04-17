//
// Created by GreenMuffin on 2025/12/7.
//

#include "hierarchy_panel.h"

#include "../cakery_event.h"

#include "runtime/core/event/event_system.h"
#include "runtime/function/world/components.h"

#include <cstring>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace cakery {

    void HierarchyPanel::setContext(dodoe::Scene* context) {
        context_ = context;
    }

    void HierarchyPanel::draw() {
        ImGui::Begin("Hierarchy");

        if (!context_) {
            ImGui::End();
            return;
        }

        for (const auto& entities = context_->getEntities(); const auto& entity : entities) {
            drawEntityNode(entity);
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered()) {
                dodoe::EventSystem::enqueueEvent<NonSelectEntityEvent>();
            }

            if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
                if (ImGui::MenuItem("Create Entity")) {
                    context_->create_entity("Entity");
                }
                ImGui::EndPopup();
            }
        }

        ImGui::End();
    }

    void HierarchyPanel::drawEntityNode(dodoe::Entity entity) {
        if (!entity || !context_->registry().valid(entity)) {
            return;
        }

        const auto& name = entity.name();
        static char edit_name_buf[256]{};

        ImGui::PushID(static_cast<int>(static_cast<dodoe::ui32>(entity)));

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        ImGui::TreeNodeEx("##entity_node", flags, "%s", name.c_str());

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            dodoe::EventSystem::enqueueEvent<SelectEntityEvent>(entity);
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            editing_handle_ = static_cast<dodoe::ui32>(entity);
            {
                const size_t copy_len = (std::min)(name.size(), sizeof(edit_name_buf) - 1);
                std::memcpy(edit_name_buf, name.data(), copy_len);
                edit_name_buf[copy_len] = '\0';
            }
            ImGui::OpenPopup("EditEntityPopup");
        }

        bool request_delete = false;
        if (ImGui::BeginPopupContextItem("EntityContext")) {
            if (ImGui::MenuItem("Delete")) {
                request_delete = true;
            }
            ImGui::EndPopup();
        }

        if (editing_handle_ == static_cast<dodoe::ui32>(entity) && ImGui::BeginPopup("EditEntityPopup")) {
            ImGui::SetKeyboardFocusHere(0);
            const bool name_changed = ImGui::InputText("##edit_name", edit_name_buf, sizeof(edit_name_buf), ImGuiInputTextFlags_EnterReturnsTrue);
            if (name_changed || ImGui::IsItemDeactivatedAfterEdit()) {
                entity.getComponent<dodoe::IDComponent>().setName(edit_name_buf);
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                editing_handle_ = 0;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        } else if (editing_handle_ == static_cast<dodoe::ui32>(entity) && !ImGui::IsPopupOpen("EditEntityPopup")) {
            editing_handle_ = 0;
        }

        if (request_delete) {
            dodoe::EventSystem::enqueueEvent<NonSelectEntityEvent>();
            context_->destroy_entity(entity);
        }

        ImGui::PopID();
    }

}
