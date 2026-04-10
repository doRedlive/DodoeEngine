//
// Created by GreenMuffin on 2025/12/12.
//

#include "inspector_panel.h"

#include "cakery/cakery_helper.h"

#include "runtime/core/world/components.h"
#include "runtime/core/meta/meta.h"
#include "runtime/core/meta/component_db.h"
#include "runtime/core/utils/common.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace cakery {

    InspectorPanel::InspectorPanel() {
        initialize_component_drawers_();
    }

    void InspectorPanel::on_ui_render() {
        ImGui::Begin("Properties");

        draw_components_();

        ImGui::End();
    }

    void InspectorPanel::draw_components_() {
        const auto game_object = g_cakery_helper.get_selected_game_object();
        if (!game_object) return;

        // Name
        {
            char buffer[256];
            std::snprintf(buffer, sizeof(buffer), "%s", game_object->get_name().c_str());
            if (ImGui::InputText("Name", buffer, sizeof(buffer))) {
                game_object->set_name(std::string(buffer));
            }
        }

        // Tag
        if (game_object->has_component<dodoe::TagComponent>()) {
            auto& tag_comp = game_object->get_component<dodoe::TagComponent>();
            char buffer[256];
            std::snprintf(buffer, sizeof(buffer), "%s", tag_comp.tag.c_str());
            if (ImGui::InputText("Tag", buffer, sizeof(buffer))) {
                //TODO: COMMAND;
                tag_comp.tag = std::string(buffer);
            }
        }

        // Other components
        for (auto& component : game_object->get_all_components()) {
            auto comp_name = std::string(component.name());
            dodoe::name_remove_namespace(comp_name);
            if (comp_name == "TagComponent") {
                continue;
            }
            component_ui_drawer_["TreeNodePush"]("<" + comp_name + ">", nullptr);
            draw_node_ui_(comp_name);
            component_ui_drawer_["TreeNodePop"]("<" + comp_name + ">", nullptr);
        }

        // Buttons
        draw_buttons_(game_object);
    }

    void InspectorPanel::initialize_component_drawers_() {
        component_ui_drawer_["TreeNodePush"] = [this](const std::string& name, void* value) -> void {
            static ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings;
            bool node_state = false;
            node_depth_++;
            if (node_depth_ > 0) {
                if (node_state_array[node_depth_ - 1].second) {
                    node_state = ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
                } else {
                    node_state_array.emplace_back(std::pair(name.c_str(), node_state));
                    return;
                }
            } else {
                node_state = ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
            }
            node_state_array.emplace_back(std::pair(name.c_str(), node_state));
        };

        component_ui_drawer_["TreeNodePop"] = [this](const std::string& name, void* value) -> void {
            if (node_state_array[node_depth_].second) {
                ImGui::TreePop();
            }
            node_state_array.pop_back();
            node_depth_--;
        };

        component_ui_drawer_["bool"] = [this](const std::string& name, void* value)  -> void {
            if(node_depth_ == -1) {
                const std::string label = "##" + name;
                ImGui::Text("%s", name.c_str());
                ImGui::SameLine();
                ImGui::Checkbox(label.c_str(), static_cast<bool*>(value));
            } else {
                if(node_state_array[node_depth_].second) {
                    const std::string full_label = "##" /*+ getLeafUINodeParentLabel() */+ name;
                    ImGui::Text("%s", name.c_str());
                    ImGui::Checkbox(full_label.c_str(), static_cast<bool*>(value));
                }
            }
        };

        component_ui_drawer_["int"] = [this](const std::string& name, void* value) -> void {
            if (node_depth_ == -1) {
                const std::string label = "##" + name;
                ImGui::Text("%s", name.c_str());
                ImGui::SameLine();
                ImGui::InputInt(label.c_str(), static_cast<int*>(value));
            } else {
                if (node_state_array[node_depth_].second) {
                    const std::string full_label = "##" /*+ getLeafUINodeParentLabel() */+ name;
                    ImGui::Text("%s", (name + ":").c_str());
                    ImGui::InputInt(full_label.c_str(), static_cast<int*>(value));
                }
            }
        };

        component_ui_drawer_["float"] = [this](const std::string& name, void* value) -> void {
            if (node_depth_ == -1) {
                const std::string label = "##" + name;
                ImGui::Text("%s", name.c_str());
                ImGui::SameLine();
                ImGui::InputFloat(label.c_str(), static_cast<float*>(value));
            } else {
                if (node_state_array[node_depth_].second) {
                    const std::string full_label = "##" /*+ getLeafUINodeParentLabel() */+ name;
                    ImGui::Text("%s", (name + ":").c_str());
                    ImGui::InputFloat(full_label.c_str(), static_cast<float*>(value));
                }
            }
        };

        component_ui_drawer_["string"] = [this](const std::string& name, void* value) -> void {
            auto* target = static_cast<std::string*>(value);
            char buffer[256];
            std::snprintf(buffer, sizeof(buffer), "%s", target->c_str());
            if (node_depth_ == -1) {
                const std::string label = "##" + name;
                ImGui::Text("%s", name.c_str());
                ImGui::SameLine();
                if (ImGui::InputText(label.c_str(), buffer, sizeof(buffer))) {
                    *target = std::string(buffer);
                }
            } else {
                if (node_state_array[node_depth_].second) {
                    const std::string full_label = "##" /*+ getLeafUINodeParentLabel() */+ name;
                    ImGui::Text("%s", (name + ":").c_str());
                    if (ImGui::InputText(full_label.c_str(), buffer, sizeof(buffer))) {
                        *target = std::string(buffer);
                    }
                }
            }
        };

        component_ui_drawer_["Vector2f"] = [this](const std::string& name, void* value) -> void {
            auto* v2 = static_cast<dodoe::Vector2f*>(value);
            float arr[2] = { v2->x, v2->y };
            if (node_depth_ == -1) {
                const std::string label = "##" + name;
                ImGui::Text("%s", name.c_str());
                ImGui::SameLine();
                if (ImGui::InputFloat2(label.c_str(), arr)) {
                    v2->x = arr[0]; v2->y = arr[1];
                }
            }
            else {
                if (node_state_array[node_depth_].second) {
                    const std::string full_label = "##" + name;
                    ImGui::Text("%s", (name + ":").c_str());
                    if (ImGui::InputFloat2(full_label.c_str(), arr)) {
                        v2->x = arr[0]; v2->y = arr[1];
                    }
                }
            }
        };

        component_ui_drawer_["Vector3f"] = [this](const std::string& name, void* value) -> void {
            auto* v3 = static_cast<dodoe::Vector3f*>(value);
            float arr[3] = { v3->x, v3->y, v3->z };
            if (node_depth_ == -1) {
                const std::string label = "##" + name;
                ImGui::Text("%s", name.c_str());
                ImGui::SameLine();
                if (ImGui::InputFloat3(label.c_str(), arr)) {
                    v3->x = arr[0]; v3->y = arr[1]; v3->z = arr[2];
                }
            } else {
                if (node_state_array[node_depth_].second) {
                    const std::string full_label = "##" + name;
                    ImGui::Text("%s", (name + ":").c_str());
                    if (ImGui::InputFloat3(full_label.c_str(), arr)) {
                        v3->x = arr[0]; v3->y = arr[1]; v3->z = arr[2];
                    }
                }
            }
        };

        component_ui_drawer_["Color"] = [this](const std::string& name, void* value) -> void {
            auto* color = static_cast<dodoe::Color*>(value);
            float arr[4] = { color->r, color->g, color->b, color->a };
            if (node_depth_ == -1) {
                const std::string label = "##" + name;
                ImGui::Text("%s", name.c_str());
                ImGui::SameLine();
                if (ImGui::ColorEdit4(label.c_str(), arr)) {
                    color->r = arr[0]; color->g = arr[1]; color->b = arr[2]; color->a = arr[3];
                }
            } else {
                if (node_state_array[node_depth_].second) {
                    const std::string full_label = "##" + name;
                    ImGui::Text("%s", (name + ":").c_str());
                    if (ImGui::ColorEdit4(full_label.c_str(), arr)) {
                        color->r = arr[0]; color->g = arr[1]; color->b = arr[2]; color->a = arr[3];
                    }
                }
            }
        };
    }

    void InspectorPanel::draw_node_ui_(const std::string& comp_name) {
        const auto game_object = g_cakery_helper.get_selected_game_object();
        if (!game_object) return;

        auto* comp_ptr = game_object->get_component_value_ptr(comp_name);
        if (!comp_ptr) {
            DoDebug("Could not found {} component value ptr", comp_name);
            return;
        }

        const auto type = entt::resolve(dodoe::String2Hash(comp_name));
        if (!type) {
            DoDebug("Could not found {} component meta type.", comp_name);
            return;
        }

        auto any = type.from_void(comp_ptr);
        if (!any) {
            DoDebug("Could not create meta_any form comp_ptr");
            return;
        }

        for (auto range = type.data(); const auto& data : range | std::views::values) {
            const auto* name_ptr = static_cast<const std::string*>(data.custom());
            const std::string field_name = name_ptr ? *name_ptr : std::string{};

            if (const auto field_info = data.type().info(); field_info == entt::type_id<bool>()) {
                // bool
                bool value = data.get(entt::meta_handle{any}).cast<bool>();
                component_ui_drawer_["bool"](field_name, &value);
                data.set(entt::meta_handle{any}, value);
            } else if (field_info == entt::type_id<int>()) {
                // int
                int value = data.get(entt::meta_handle{any}).cast<int>();
                component_ui_drawer_["int"](field_name, &value);
                data.set(entt::meta_handle{any}, value);
            } else if (field_info == entt::type_id<float>()) {
                // float
                float value = data.get(entt::meta_handle{any}).cast<float>();
                component_ui_drawer_["float"](field_name, &value);
                data.set(entt::meta_handle{any}, value);
            } else if (field_info == entt::type_id<std::string>()) {
                // string
                auto value = data.get(entt::meta_handle{any}).cast<std::string>();
                component_ui_drawer_["string"](field_name, &value);
                data.set(entt::meta_handle{any}, value);
            } else if (field_info == entt::type_id<dodoe::Vector3f>()) {
                // Vector3f
                auto value = data.get(entt::meta_handle{any}).cast<dodoe::Vector3f>();
                component_ui_drawer_["Vector3f"](field_name, &value);
                data.set(entt::meta_handle{any}, value);
            } else if (field_info == entt::type_id<dodoe::Vector2f>()) {
                // Vector2f
                auto value = data.get(entt::meta_handle{any}).cast<dodoe::Vector2f>();
                component_ui_drawer_["Vector2f"](field_name, &value);
                data.set(entt::meta_handle{any}, value);
            } else if (field_info == entt::type_id<dodoe::Color>()) {
                // Coloe
                auto value = data.get(entt::meta_handle{any}).cast<dodoe::Color>();
                component_ui_drawer_["Color"](field_name, &value);
                data.set(entt::meta_handle{any}, value);
            }
        }
    }

    void InspectorPanel::draw_buttons_(dodoe::GameObject* game_object) const {
        if (ImGui::Button("Add Component")) {
            ImGui::OpenPopup("AddComponent_Popup");
        }

        if (ImGui::BeginPopup("AddComponent_Popup")) {
            for (const auto& [comp_type, name] : dodoe::ComponentDB::instance().all_components()) {
                if (const bool has = game_object->has_component(name); !has && ImGui::MenuItem(name.c_str())) {
                    game_object->add_component(name);
                }
            }
            ImGui::EndPopup();
        }
    }

} // cakery
