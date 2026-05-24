//
// Created by GreenMuffin on 2025/12/7.
//

#include "hierarchy_panel.h"

#include "../cakery_event.h"

#include "runtime/core/event/event_system.h"
#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/script/script_system.h"
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

        auto entities = context_->getEntities();
        std::unordered_set<dodoe::ui32> visited;
        for (auto entity : entities) {
            if (!entity || !context_->registry().valid(entity)) {
                continue;
            }

            if (entity.hasComponent<dodoe::HierarchyComponent>()) {
                const auto& hierarchy = entity.getComponent<dodoe::HierarchyComponent>();
                if (hierarchy.parent && hierarchy.parent.valid()) {
                    continue;
                }
            }

            drawEntityNode(entity, visited);
        }

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            && ImGui::IsWindowHovered()
            && !ImGui::IsAnyItemHovered()) {
            dodoe::EventSystem::Enqueue<NonSelectEntityEvent>();
        }

        if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem("Create Entity")) {
                context_->createEntity("Entity");
            }
            ImGui::EndPopup();
        }

        ImGui::End();
    }

    void HierarchyPanel::drawEntityNode(dodoe::Entity entity, std::unordered_set<dodoe::ui32>& visited) {
        if (!entity || !context_->registry().valid(entity)) {
            return;
        }

        const auto entity_id = static_cast<dodoe::ui32>(entity);
        if (!visited.insert(entity_id).second) {
            return;
        }

        const auto& name = entity.name();
        static char edit_name_buf[256]{};

        ImGui::PushID(static_cast<int>(entity_id));

        const bool has_children = entity.hasComponent<dodoe::HierarchyComponent>()
            && !entity.getComponent<dodoe::HierarchyComponent>().children.empty();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
            | ImGuiTreeNodeFlags_OpenOnDoubleClick
            | ImGuiTreeNodeFlags_SpanAvailWidth
            | ImGuiTreeNodeFlags_FramePadding;
        if (!has_children) {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        const bool open = ImGui::TreeNodeEx("##entity_node", flags, "%s", name.c_str());

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            dodoe::EventSystem::Enqueue<SelectEntityEvent>(entity);
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
            if (auto& app = dodoe::Application::Self(); app.context().script_system) {
                if (auto* runtime = app.context().script_system->getMonoRuntime()) {
                    runtime->removeEntityFromManagedWorld(static_cast<uint64_t>(entity.uuid()));
                }
            }
            dodoe::EventSystem::Enqueue<NonSelectEntityEvent>();
            context_->destroyEntity(entity);
        }

        if (open && has_children) {
            auto& hierarchy = entity.getComponent<dodoe::HierarchyComponent>();
            for (const auto& child : hierarchy.children) {
                drawEntityNode(child, visited);
            }
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

}
