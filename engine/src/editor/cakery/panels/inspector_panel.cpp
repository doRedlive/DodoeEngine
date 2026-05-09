//
// Created by GreenMuffin on 2025/12/12.
//

#include "inspector_panel.h"

#include "runtime/core/utils/common.h"
#include "runtime/core/project/project.h"
#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/core/meta/component_db.h"
#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/event/event_system.h"
#include "runtime/function/script/script_system.h"
#include "runtime/function/script/script_runtime.h"
#include "runtime/function/script/script_class.h"
#include "runtime/function/world/components.h"
#include "runtime/function/world/world.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/resource/asset/asset.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/function/render/framework/texture_manager.h"
#include "runtime/function/render/render_system.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include <cstring>
#include <algorithm>
#include <unordered_set>

namespace cakery {

    namespace {
        std::string GetMonoClassShortName(const dodoe::ScriptClass& script_class) {
            MonoClass* mono_class = script_class.getMonoClass();
            if (!mono_class) {
                return "<UnknownMonoComponent>";
            }

            const char* name = mono_class_get_name(mono_class);
            return name ? std::string(name) : "<UnknownMonoComponent>";
        }

        std::string GetShortTypeName(const std::string& full_name) {
            const size_t pos = full_name.find_last_of('.');
            if (pos == std::string::npos) {
                return full_name;
            }
            return full_name.substr(pos + 1);
        }

        std::string GetMonoClassFullName(const dodoe::ScriptClass& script_class) {
            MonoClass* mono_class = script_class.getMonoClass();
            if (!mono_class) {
                return "<UnknownMonoComponent>";
            }

            const char* ns = mono_class_get_namespace(mono_class);
            const char* name = mono_class_get_name(mono_class);
            if (!name) {
                return "<UnknownMonoComponent>";
            }

            return (ns && strlen(ns) > 0) ? fmt::format("{}.{}", ns, name) : std::string(name);
        }

        const char* AssetTypeToString(const dodoe::AssetType type) {
            switch (type) {
                case dodoe::AssetType::Scene: return "Scene";
                case dodoe::AssetType::Texture: return "Texture";
                case dodoe::AssetType::Model: return "Model";
                case dodoe::AssetType::Shader: return "Shader";
                default: return "None";
            }
        }

        std::vector<dodoe::AssetRef> CollectAssetsByType(const dodoe::AssetType type) {
            auto* asset_manager = dodoe::ResourceManager::self().asset_manager();
            if (!asset_manager) {
                return {};
            }
            return asset_manager->getAssetsByType(type);
        }

        dodoe::TextureManager* GetTextureManager() {
            auto& app = dodoe::Application::Self();
            auto* render_system = app.context().render_system.get();
            return render_system ? render_system->getTextureManager() : nullptr;
        }

        void DrawAssetTexturePreview(const dodoe::AssetRef& asset_ref) {
            if (asset_ref.type != dodoe::AssetType::Texture || asset_ref.path_id == 0) {
                return;
            }

            auto* texture_manager = GetTextureManager();
            if (!texture_manager) {
                return;
            }

            auto texture = texture_manager->loadTexture(asset_ref.path_id);
            if (!texture || !texture->handle) {
                return;
            }

            const ImTextureRef preview_ref(reinterpret_cast<ImTextureID>(texture->handle.Get()));
            ImGui::Image(preview_ref, ImVec2(72.0f, 72.0f), ImVec2(0, 1), ImVec2(1, 0));
        }
    }

    InspectorPanel::InspectorPanel() {
        dodoe::EventSystem::Subscribe<SelectEntityEvent, &InspectorPanel::onSelectEntity>(this);
        dodoe::EventSystem::Subscribe<NonSelectEntityEvent, &InspectorPanel::onNonSelectEntity>(this);
        initializeComponentDrawers();
    }

    InspectorPanel::~InspectorPanel() {
        dodoe::EventSystem::Unsubscribe<SelectEntityEvent, &InspectorPanel::onSelectEntity>(this);
        dodoe::EventSystem::Unsubscribe<NonSelectEntityEvent, &InspectorPanel::onNonSelectEntity>(this);
    }

    void InspectorPanel::markCurrentComponentDirty() {
        if (!m_selected_entity.valid() || m_current_component_name.empty()) {
            return;
        }

        (void)dodoe::ComponentDB::self().markComponentDirty(m_selected_entity, m_current_component_name);
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

        auto& app = dodoe::Application::Self();
        dodoe::Scene* scene = app.context().world ? app.context().world->getCurrentScene() : nullptr;
        if (!scene || !scene->registry().valid(m_selected_entity)) {
            m_selected_entity = dodoe::Entity::NullEntity();
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

        drawMonoComponents();

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
                if (ImGui::Checkbox(label.c_str(), static_cast<bool*>(value))) {
                    markCurrentComponentDirty();
                }
            } else {
                if(m_node_state_array[m_node_depth].second) {
                    const std::string full_label = "##" /*+ getLeafUINodeParentLabel() */+ name;
                    ImGui::Text("%s", name.c_str());
                    if (ImGui::Checkbox(full_label.c_str(), static_cast<bool*>(value))) {
                        markCurrentComponentDirty();
                    }
                }
            }
        };

        m_component_ui_drawer["int"] = [this](const std::string& name, void* value) -> void {
            if (m_node_depth == -1) {
                const std::string label = "##" + name;
                ImGui::Text("%s", name.c_str());
                ImGui::SameLine();
                if (ImGui::InputInt(label.c_str(), static_cast<int*>(value))) {
                    markCurrentComponentDirty();
                }
            } else {
                if (m_node_state_array[m_node_depth].second) {
                    const std::string full_label = "##" /*+ getLeafUINodeParentLabel() */+ name;
                    ImGui::Text("%s", (name + ":").c_str());
                    if (ImGui::InputInt(full_label.c_str(), static_cast<int*>(value))) {
                        markCurrentComponentDirty();
                    }
                }
            }
        };

        m_component_ui_drawer["float"] = [this](const std::string& name, void* value) -> void {
            if (m_node_depth == -1) {
                const std::string label = "##" + name;
                ImGui::Text("%s", name.c_str());
                ImGui::SameLine();
                if (ImGui::InputFloat(label.c_str(), static_cast<float*>(value))) {
                    markCurrentComponentDirty();
                }
            } else {
                if (m_node_state_array[m_node_depth].second) {
                    const std::string full_label = "##" /*+ getLeafUINodeParentLabel() */+ name;
                    ImGui::Text("%s", (name + ":").c_str());
                    if (ImGui::InputFloat(full_label.c_str(), static_cast<float*>(value))) {
                        markCurrentComponentDirty();
                    }
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
                    markCurrentComponentDirty();
                }
            } else {
                if (m_node_state_array[m_node_depth].second) {
                    const std::string full_label = "##" /*+ getLeafUINodeParentLabel() */+ name;
                    ImGui::Text("%s", (name + ":").c_str());
                    if (ImGui::InputText(full_label.c_str(), buffer, sizeof(buffer))) {
                        *target = std::string(buffer);
                        markCurrentComponentDirty();
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
                    markCurrentComponentDirty();
                }
            }
            else {
                if (m_node_state_array[m_node_depth].second) {
                    const std::string full_label = "##" + name;
                    ImGui::Text("%s", (name + ":").c_str());
                    if (ImGui::InputFloat2(full_label.c_str(), arr)) {
                        v2->x = arr[0]; v2->y = arr[1];
                        markCurrentComponentDirty();
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
                    markCurrentComponentDirty();
                }
            } else {
                if (m_node_state_array[m_node_depth].second) {
                    const std::string full_label = "##" + name;
                    ImGui::Text("%s", (name + ":").c_str());
                    if (ImGui::InputFloat3(full_label.c_str(), arr)) {
                        v3->x = arr[0]; v3->y = arr[1]; v3->z = arr[2];
                        markCurrentComponentDirty();
                    }
                }
            }
        };

        m_component_ui_drawer["Uuid"] = [this](const std::string& name, void* value) -> void {
            auto* uuid = static_cast<dodoe::Uuid*>(value);
            if (!uuid) {
                return;
            }

            uint64_t raw = static_cast<uint64_t>(*uuid);
            if (m_node_depth == -1) {
                const std::string label = "##" + name;
                ImGui::Text("%s", name.c_str());
                ImGui::SameLine();
                if (ImGui::InputScalar(label.c_str(), ImGuiDataType_U64, &raw)) {
                    *uuid = dodoe::Uuid(raw);
                    markCurrentComponentDirty();
                }
            } else if (m_node_state_array[m_node_depth].second) {
                const std::string full_label = "##" + name;
                ImGui::Text("%s", (name + ":").c_str());
                if (ImGui::InputScalar(full_label.c_str(), ImGuiDataType_U64, &raw)) {
                    *uuid = dodoe::Uuid(raw);
                    markCurrentComponentDirty();
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
                    markCurrentComponentDirty();
                }
            } else {
                if (m_node_state_array[m_node_depth].second) {
                    const std::string full_label = "##" + name;
                    ImGui::Text("%s", (name + ":").c_str());
                    if (ImGui::ColorEdit4(full_label.c_str(), arr)) {
                        color->r = arr[0]; color->g = arr[1]; color->b = arr[2]; color->a = arr[3];
                        markCurrentComponentDirty();
                    }
                }
            }
        };

        m_component_ui_drawer["AssetRef"] = [this](const std::string& name, void* value) -> void {
            auto* asset_ref = static_cast<dodoe::AssetRef*>(value);
            if (!asset_ref) {
                return;
            }

            if (m_node_depth >= 0 && !m_node_state_array[m_node_depth].second) {
                return;
            }

            const std::string type_label = "##AssetType_" + name;
            const std::string picker_popup = "##AssetPicker_" + name;
            const std::string select_button = "Select Asset##" + name;

            ImGui::Text("%s", (name + ":").c_str());
            if (ImGui::BeginCombo(type_label.c_str(), AssetTypeToString(asset_ref->type))) {
                const dodoe::AssetType options[] = {
                    dodoe::AssetType::None,
                    dodoe::AssetType::Texture,
                    dodoe::AssetType::Scene,
                    dodoe::AssetType::Model,
                    dodoe::AssetType::Shader
                };

                for (const auto option : options) {
                    const bool selected = (asset_ref->type == option);
                    if (ImGui::Selectable(AssetTypeToString(option), selected)) {
                        if (asset_ref->type != option) {
                            asset_ref->type = option;
                            asset_ref->path.clear();
                            asset_ref->path_id = 0;
                            asset_ref->handle = dodoe::AssetHandle{};
                            markCurrentComponentDirty();
                        }
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            const std::string current_label = asset_ref->path.empty()
                                                  ? std::string("<None>")
                                                  : std::filesystem::path(asset_ref->path).filename().string();
            ImGui::Text("Current: %s", current_label.c_str());

            if (asset_ref->type == dodoe::AssetType::Texture) {
                DrawAssetTexturePreview(*asset_ref);
                if (ImGui::Button(select_button.c_str())) {
                    ImGui::OpenPopup(picker_popup.c_str());
                }

                if (ImGui::BeginPopupModal(picker_popup.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                    if (ImGui::Button("Clear")) {
                        asset_ref->path.clear();
                        asset_ref->path_id = 0;
                        asset_ref->handle = dodoe::AssetHandle{};
                        markCurrentComponentDirty();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Close")) {
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::Separator();
                    const auto assets = CollectAssetsByType(dodoe::AssetType::Texture);
                    for (const auto& asset : assets) {
                        const std::string filename = std::filesystem::path(asset.path).filename().string();
                        const std::string option_label = filename + "##" + asset.path;
                        if (ImGui::Selectable(option_label.c_str(), asset_ref->path == asset.path)) {
                            asset_ref->type = dodoe::AssetType::Texture;
                            asset_ref->path = asset.path;
                            asset_ref->path_id = asset.path_id;
                            asset_ref->handle = asset.handle;
                            markCurrentComponentDirty();
                            ImGui::CloseCurrentPopup();
                        }
                    }

                    ImGui::EndPopup();
                }
            } else {
                ImGui::TextDisabled("Current type has no picker yet.");
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

        m_current_component_name = comp_name;

        for (int i = 0; i < field_count; ++i) {
            auto& field = fields[i];
            const char* field_name_cstr = field.getFieldName();
            const char* field_type_cstr = field.getFieldTypeName();

            if (field_name_cstr == nullptr || field_type_cstr == nullptr) {
                continue;
            }

            std::string field_name = field_name_cstr;
            std::string field_type = field_type_cstr;

            dodoe::NameRemoveNamespace(field_type);
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

        m_current_component_name.clear();
        delete[] fields;
    }

    void InspectorPanel::drawMonoComponents() {
        if (!m_selected_entity.valid()) {
            return;
        }

        auto& app = dodoe::Application::Self();
        auto* context_script_system = app.context().script_system.get();
        if (!context_script_system) {
            return;
        }

        auto* runtime = context_script_system->getMonoRuntime();
        if (!runtime) {
            return;
        }

        const uint64_t entity_uuid = static_cast<uint64_t>(m_selected_entity.uuid());
        runtime->loadEntityMonoComponentsFromManaged(entity_uuid);

        const auto& mono_components = runtime->getComponentInstanceUmap();
        auto it = mono_components.find(entity_uuid);
        if (it == mono_components.end()) {
            return;
        }

        for (auto& instance_ref : it->second) {
            if (!instance_ref) {
                continue;
            }

            auto script_class = instance_ref->getScriptClass();
            if (!script_class) {
                continue;
            }

            const std::string component_name = GetMonoClassShortName(*script_class);
            m_component_ui_drawer["TreeNodePush"]("<" + component_name + ">", nullptr);
            drawMonoNode(component_name, *instance_ref, *script_class);
            m_component_ui_drawer["TreeNodePop"]("<" + component_name + ">", nullptr);
        }
    }

    void InspectorPanel::drawMonoNode(const std::string& comp_name, dodoe::MonoComponentInstance& component_instance, dodoe::ScriptClass& script_class) {
        (void)comp_name;
        if (m_node_depth >= 0 && m_node_depth < static_cast<int>(m_node_state_array.size())
            && !m_node_state_array[m_node_depth].second) {
            return;
        }

        const auto& fields = script_class.getFields();
        for (const auto& [field_name, field] : fields) {
            const std::string label = "##Mono_" + field_name;
            switch (field.type) {
                case dodoe::ScriptFieldType::Bool: {
                    bool value = component_instance.getFieldValue<bool>(field_name);
                    if (ImGui::Checkbox(label.c_str(), &value)) {
                        component_instance.setFieldValue<bool>(field_name, value);
                    }
                    ImGui::SameLine();
                    ImGui::Text("%s", field_name.c_str());
                    break;
                }
                case dodoe::ScriptFieldType::Float: {
                    float value = component_instance.getFieldValue<float>(field_name);
                    if (ImGui::InputFloat(label.c_str(), &value)) {
                        component_instance.setFieldValue<float>(field_name, value);
                    }
                    ImGui::SameLine();
                    ImGui::Text("%s", field_name.c_str());
                    break;
                }
                case dodoe::ScriptFieldType::Double: {
                    double value = component_instance.getFieldValue<double>(field_name);
                    if (ImGui::InputDouble(label.c_str(), &value)) {
                        component_instance.setFieldValue<double>(field_name, value);
                    }
                    ImGui::SameLine();
                    ImGui::Text("%s", field_name.c_str());
                    break;
                }
                case dodoe::ScriptFieldType::Int:
                case dodoe::ScriptFieldType::Short:
                case dodoe::ScriptFieldType::Byte: {
                    int value = component_instance.getFieldValue<int>(field_name);
                    if (ImGui::InputInt(label.c_str(), &value)) {
                        component_instance.setFieldValue<int>(field_name, value);
                    }
                    ImGui::SameLine();
                    ImGui::Text("%s", field_name.c_str());
                    break;
                }
                case dodoe::ScriptFieldType::UInt:
                case dodoe::ScriptFieldType::UShort:
                case dodoe::ScriptFieldType::UByte: {
                    int value = static_cast<int>(component_instance.getFieldValue<uint32_t>(field_name));
                    if (ImGui::InputInt(label.c_str(), &value)) {
                        component_instance.setFieldValue<uint32_t>(field_name, static_cast<uint32_t>(value < 0 ? 0 : value));
                    }
                    ImGui::SameLine();
                    ImGui::Text("%s", field_name.c_str());
                    break;
                }
                case dodoe::ScriptFieldType::Long: {
                    int64_t value = component_instance.getFieldValue<int64_t>(field_name);
                    if (ImGui::InputScalar(label.c_str(), ImGuiDataType_S64, &value)) {
                        component_instance.setFieldValue<int64_t>(field_name, value);
                    }
                    ImGui::SameLine();
                    ImGui::Text("%s", field_name.c_str());
                    break;
                }
                case dodoe::ScriptFieldType::ULong:
                case dodoe::ScriptFieldType::Entity: {
                    uint64_t value = component_instance.getFieldValue<uint64_t>(field_name);
                    if (ImGui::InputScalar(label.c_str(), ImGuiDataType_U64, &value)) {
                        component_instance.setFieldValue<uint64_t>(field_name, value);
                    }
                    ImGui::SameLine();
                    ImGui::Text("%s", field_name.c_str());
                    break;
                }
                case dodoe::ScriptFieldType::Vector2: {
                    dodoe::Vector2f value = component_instance.getFieldValue<dodoe::Vector2f>(field_name);
                    float arr[2] = { value.x, value.y };
                    if (ImGui::InputFloat2(label.c_str(), arr)) {
                        value.x = arr[0];
                        value.y = arr[1];
                        component_instance.setFieldValue<dodoe::Vector2f>(field_name, value);
                    }
                    ImGui::SameLine();
                    ImGui::Text("%s", field_name.c_str());
                    break;
                }
                case dodoe::ScriptFieldType::Vector3: {
                    dodoe::Vector3f value = component_instance.getFieldValue<dodoe::Vector3f>(field_name);
                    float arr[3] = { value.x, value.y, value.z };
                    if (ImGui::InputFloat3(label.c_str(), arr)) {
                        value.x = arr[0];
                        value.y = arr[1];
                        value.z = arr[2];
                        component_instance.setFieldValue<dodoe::Vector3f>(field_name, value);
                    }
                    ImGui::SameLine();
                    ImGui::Text("%s", field_name.c_str());
                    break;
                }
                default:
                    ImGui::Text("%s (Unsupported Type)", field_name.c_str());
                    break;
            }
        }
    }

    void InspectorPanel::drawButtons(dodoe::Entity entity) {
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

            auto& app = dodoe::Application::Self();
            auto* context_script_system = app.context().script_system.get();
            if (context_script_system) {
                auto* runtime = context_script_system->getMonoRuntime();
                if (runtime) {
                    const uint64_t entity_uuid = static_cast<uint64_t>(entity.uuid());
                    runtime->loadEntityMonoComponentsFromManaged(entity_uuid);
                    const auto& mono_components = runtime->getComponentInstanceUmap();
                    std::unordered_set<std::string> exists;
                    if (auto it = mono_components.find(entity_uuid); it != mono_components.end()) {
                        for (const auto& instance_ref : it->second) {
                            if (!instance_ref) {
                                continue;
                            }
                            auto script_class = instance_ref->getScriptClass();
                            if (!script_class) {
                                continue;
                            }
                            exists.insert(GetMonoClassFullName(*script_class));
                        }
                    }

                    for (const auto& [full_name, _] : runtime->getComponentClassUmap()) {
                        if (exists.find(full_name) != exists.end()) {
                            continue;
                        }

                        const std::string display_name = GetShortTypeName(full_name);
                        if (ImGui::MenuItem(display_name.c_str())) {
                            if (runtime->addEntityMonoComponentFromManaged(entity_uuid, full_name)) {
                                runtime->loadEntityMonoComponentsFromManaged(entity_uuid);
                            }
                        }
                    }
                }
            }
            ImGui::EndPopup();
        }
    }

    void InspectorPanel::onSelectEntity(const SelectEntityEvent& event) {
        m_selected_entity = event.select; 
    }

    void InspectorPanel::onNonSelectEntity() {
        m_selected_entity = dodoe::Entity::NullEntity();
    }

} // cakery
