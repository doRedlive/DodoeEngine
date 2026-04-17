//
// Created by GreenMuffin on 2025/12/12.
//

#include "inspector_panel.h"

#include "runtime/core/utils/common.h"
#include "runtime/core/meta/component_db.h"
#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/event/event_system.h"
#include "runtime/function/world/components.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace cakery {

    InspectorPanel::InspectorPanel() {
        dodoe::EventSystem::subscribe_event<SelectEntityEvent, &InspectorPanel::onSelectEntity>(this);
        dodoe::EventSystem::subscribe_event<NonSelectEntityEvent, &InspectorPanel::onNonSelectEntity>(this);
        initializeComponentDrawers();
    }

    InspectorPanel::~InspectorPanel() {
        dodoe::EventSystem::unsubscribe_event<SelectEntityEvent, &InspectorPanel::onSelectEntity>(this);
        dodoe::EventSystem::unsubscribe_event<NonSelectEntityEvent, &InspectorPanel::onNonSelectEntity>(this);
    }

    void InspectorPanel::draw() {
        ImGui::Begin("Inspector");

        drawComponents();

        ImGui::End();
    }

    void InspectorPanel::drawComponents() {
        if (!m_selected_entity.valid()) {
            return;
        }

        // Name
        {
            char buffer[256];
            std::snprintf(buffer, sizeof(buffer), "%s", m_selected_entity.name().c_str());
            if (ImGui::InputText("Name", buffer, sizeof(buffer))) {
                m_selected_entity.getComponent<dodoe::IDComponent>().setName(std::string(buffer));
            }
        }

        // Tag
        if (m_selected_entity.hasComponent<dodoe::TagComponent>()) {
            auto& tag_comp = m_selected_entity.getComponent<dodoe::TagComponent>();
            char buffer[256];
            std::snprintf(buffer, sizeof(buffer), "%s", tag_comp.tag.c_str());
            if (ImGui::InputText("Tag", buffer, sizeof(buffer))) {
                //TODO: COMMAND;
                tag_comp.tag = std::string(buffer);
            }
        }

        // Other components
        for (const auto& entry : dodoe::ComponentDB::self().entries()) {
            if (!entry.contains(m_selected_entity)) {
                continue;
            }

            if (entry.name == "IDComponent" || entry.name == "TagComponent") {
                continue;
            }

            const std::string comp_name = entry.name;
            m_component_ui_drawer["TreeNodePush"]("<" + comp_name + ">", nullptr);
            drawNode(comp_name);
            m_component_ui_drawer["TreeNodePop"]("<" + comp_name + ">", nullptr);
        }

        // Buttons
        drawButtons(m_selected_entity);
    }

    void InspectorPanel::initializeComponentDrawers() {
        m_component_ui_drawer["TreeNodePush"] = [this](const std::string& name, void* value) -> void {
            static ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings;
            bool node_state = false;
            m_node_depth++;
            if (m_node_depth > 0) {
                if (m_node_state_array[m_node_depth - 1].second) {
                    node_state = ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
                } else {
                    m_node_state_array.emplace_back(std::pair(name.c_str(), node_state));
                    return;
                }
            } else {
                node_state = ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
            }
            m_node_state_array.emplace_back(std::pair(name.c_str(), node_state));
        };

        m_component_ui_drawer["TreeNodePop"] = [this](const std::string& name, void* value) -> void {
            if (m_node_state_array[m_node_depth].second) {
                ImGui::TreePop();
            }
            m_node_state_array.pop_back();
            m_node_depth--;
        };

        m_component_ui_drawer["bool"] = [this](const std::string& name, void* value)  -> void {
            if(m_node_depth == -1) {
                const std::string label = "##" + name;
                ImGui::Text("%s", name.c_str());
                ImGui::SameLine();
                ImGui::Checkbox(label.c_str(), static_cast<bool*>(value));
            } else {
                if(m_node_state_array[m_node_depth].second) {
                    const std::string full_label = "##" /*+ getLeafUINodeParentLabel() */+ name;
                    ImGui::Text("%s", name.c_str());
                    ImGui::Checkbox(full_label.c_str(), static_cast<bool*>(value));
                }
            }
        };

        m_component_ui_drawer["int"] = [this](const std::string& name, void* value) -> void {
            if (m_node_depth == -1) {
                const std::string label = "##" + name;
                ImGui::Text("%s", name.c_str());
                ImGui::SameLine();
                ImGui::InputInt(label.c_str(), static_cast<int*>(value));
            } else {
                if (m_node_state_array[m_node_depth].second) {
                    const std::string full_label = "##" /*+ getLeafUINodeParentLabel() */+ name;
                    ImGui::Text("%s", (name + ":").c_str());
                    ImGui::InputInt(full_label.c_str(), static_cast<int*>(value));
                }
            }
        };

        m_component_ui_drawer["float"] = [this](const std::string& name, void* value) -> void {
            if (m_node_depth == -1) {
                const std::string label = "##" + name;
                ImGui::Text("%s", name.c_str());
                ImGui::SameLine();
                ImGui::InputFloat(label.c_str(), static_cast<float*>(value));
            } else {
                if (m_node_state_array[m_node_depth].second) {
                    const std::string full_label = "##" /*+ getLeafUINodeParentLabel() */+ name;
                    ImGui::Text("%s", (name + ":").c_str());
                    ImGui::InputFloat(full_label.c_str(), static_cast<float*>(value));
                }
            }
        };

        m_component_ui_drawer["string"] = [this](const std::string& name, void* value) -> void {
            auto* target = static_cast<std::string*>(value);
            char buffer[256];
            std::snprintf(buffer, sizeof(buffer), "%s", target->c_str());
            if (m_node_depth == -1) {
                const std::string label = "##" + name;
                ImGui::Text("%s", name.c_str());
                ImGui::SameLine();
                if (ImGui::InputText(label.c_str(), buffer, sizeof(buffer))) {
                    *target = std::string(buffer);
                }
            } else {
                if (m_node_state_array[m_node_depth].second) {
                    const std::string full_label = "##" /*+ getLeafUINodeParentLabel() */+ name;
                    ImGui::Text("%s", (name + ":").c_str());
                    if (ImGui::InputText(full_label.c_str(), buffer, sizeof(buffer))) {
                        *target = std::string(buffer);
                    }
                }
            }
        };

        m_component_ui_drawer["Vector2f"] = [this](const std::string& name, void* value) -> void {
            auto* v2 = static_cast<dodoe::Vector2f*>(value);
            float arr[2] = { v2->x, v2->y };
            if (m_node_depth == -1) {
                const std::string label = "##" + name;
                ImGui::Text("%s", name.c_str());
                ImGui::SameLine();
                if (ImGui::InputFloat2(label.c_str(), arr)) {
                    v2->x = arr[0]; v2->y = arr[1];
                }
            }
            else {
                if (m_node_state_array[m_node_depth].second) {
                    const std::string full_label = "##" + name;
                    ImGui::Text("%s", (name + ":").c_str());
                    if (ImGui::InputFloat2(full_label.c_str(), arr)) {
                        v2->x = arr[0]; v2->y = arr[1];
                    }
                }
            }
        };

        m_component_ui_drawer["Vector3f"] = [this](const std::string& name, void* value) -> void {
            auto* v3 = static_cast<dodoe::Vector3f*>(value);
            float arr[3] = { v3->x, v3->y, v3->z };
            if (m_node_depth == -1) {
                const std::string label = "##" + name;
                ImGui::Text("%s", name.c_str());
                ImGui::SameLine();
                if (ImGui::InputFloat3(label.c_str(), arr)) {
                    v3->x = arr[0]; v3->y = arr[1]; v3->z = arr[2];
                }
            } else {
                if (m_node_state_array[m_node_depth].second) {
                    const std::string full_label = "##" + name;
                    ImGui::Text("%s", (name + ":").c_str());
                    if (ImGui::InputFloat3(full_label.c_str(), arr)) {
                        v3->x = arr[0]; v3->y = arr[1]; v3->z = arr[2];
                    }
                }
            }
        };

        m_component_ui_drawer["Color"] = [this](const std::string& name, void* value) -> void {
            auto* color = static_cast<dodoe::Color*>(value);
            float arr[4] = { color->r, color->g, color->b, color->a };
            if (m_node_depth == -1) {
                const std::string label = "##" + name;
                ImGui::Text("%s", name.c_str());
                ImGui::SameLine();
                if (ImGui::ColorEdit4(label.c_str(), arr)) {
                    color->r = arr[0]; color->g = arr[1]; color->b = arr[2]; color->a = arr[3];
                }
            } else {
                if (m_node_state_array[m_node_depth].second) {
                    const std::string full_label = "##" + name;
                    ImGui::Text("%s", (name + ":").c_str());
                    if (ImGui::ColorEdit4(full_label.c_str(), arr)) {
                        color->r = arr[0]; color->g = arr[1]; color->b = arr[2]; color->a = arr[3];
                    }
                }
            }
        };
    }

    void InspectorPanel::drawNode(const std::string& comp_name) {
        if (!m_selected_entity.valid()) {
            return;
        }

        auto* comp_ptr = dodoe::ComponentDB::self().getComponentPtr(m_selected_entity, comp_name);
        if (!comp_ptr) {
            DoDebug("Could not found {} component value ptr", comp_name);
            return;
        }

        auto comp_meta = dodoe::TypeMeta::newMetaFromName(comp_name);
        if (!comp_meta.isValid()) {
            DoDebug("Could not found {} reflection meta type.", comp_name);
            return;
        }

        dodoe::FieldAccessor* fields = nullptr;
        const int field_count = comp_meta.get_field_list(fields);
        if (field_count <= 0 || fields == nullptr) {
            return;
        }

        for (int i = 0; i < field_count; ++i) {
            auto& field = fields[i];
            const char* field_name_cstr = field.getFieldName();
            const char* field_type_cstr = field.getFieldTypeName();

            if (field_name_cstr == nullptr || field_type_cstr == nullptr) {
                continue;
            }

            std::string field_name = field_name_cstr;
            std::string field_type = field_type_cstr;

            dodoe::name_remove_namespace(field_type);
            if (field_type.find("string") != std::string::npos) {
                field_type = "string";
            }

            void* field_ptr = field.get(comp_ptr);
            if (!field_ptr) { continue; }

            const auto it = m_component_ui_drawer.find(field_type);
            if (it != m_component_ui_drawer.end()) {
                it->second(field_name, field_ptr);
            }
        }

        delete[] fields;
    }

    void InspectorPanel::drawButtons(dodoe::Entity entity) const {
        if (!entity.valid()) {
            return;
        }

        if (ImGui::Button("Add Component")) {
            ImGui::OpenPopup("AddComponent_Popup");
        }

        if (ImGui::BeginPopup("AddComponent_Popup")) {
            for (const auto& [comp_type, name] : dodoe::ComponentDB::self().allComponents()) {
                (void)comp_type;
                if (!dodoe::ComponentDB::self().hasComponent(entity, name) && ImGui::MenuItem(name.c_str())) {
                    dodoe::ComponentDB::self().addComponent(entity, name);
                }
            }
            ImGui::EndPopup();
        }
    }

    void InspectorPanel::onSelectEntity(const SelectEntityEvent& event) {
        m_selected_entity = event.select; 
    }

    void InspectorPanel::onNonSelectEntity() {
        m_selected_entity = dodoe::Entity::nullEntity();
    }

} // cakery
