#include "debug_imgui.h"

#ifdef DODOE_DEBUG_ENABLED

#include "imgui/imgui.h"

#include "runtime/core/context/system_context.h"
#include "runtime/core/meta/component_db.h"
#include "runtime/function/script/script_system.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/world/world.h"

#include <cstdint>
#include <array>
#include <cstdio>
#include <functional>

namespace dodoe {

    namespace {
        using UUIDSet = UnorderedSet<UUID>;

        bool DrawJsonValue(Json& value, bool read_only);

        bool DrawJsonMember(const String& name, Json& value, bool read_only) {
            ImGui::PushID(name.c_str());
            const bool immutable_id = read_only && name == "id";
            bool changed = false;

            if (value.is_object() || value.is_array()) {
                const bool open = ImGui::TreeNodeEx("##value", ImGuiTreeNodeFlags_SpanAvailWidth,
                                                    "%s", name.c_str());
                if (open) {
                    changed = DrawJsonValue(value, read_only);
                    ImGui::TreePop();
                }
            }
            else {
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(name.c_str());
                ImGui::SameLine();
                if (immutable_id) {
                    ImGui::TextDisabled("%s", value.dump().c_str());
                }
                else {
                    changed = DrawJsonValue(value, false);
                }
            }

            ImGui::PopID();
            return changed;
        }

        bool DrawJsonValue(Json& value, bool read_only) {
            if (value.is_boolean()) {
                bool v = value.get<bool>();
                if (read_only) {
                    ImGui::TextDisabled("%s", v ? "true" : "false");
                    return false;
                }
                if (ImGui::Checkbox("##value", &v)) {
                    value = v;
                    return true;
                }
                return false;
            }
            if (value.is_number_integer()) {
                ImS64 v = value.get<ImS64>();
                if (read_only) {
                    ImGui::TextDisabled("%lld", static_cast<long long>(v));
                    return false;
                }
                if (ImGui::InputScalar("##value", ImGuiDataType_S64, &v)) {
                    value = static_cast<int64_t>(v);
                    return true;
                }
                return false;
            }
            if (value.is_number_unsigned()) {
                ImU64 v = value.get<ImU64>();
                if (read_only) {
                    ImGui::TextDisabled("%llu", static_cast<unsigned long long>(v));
                    return false;
                }
                if (ImGui::InputScalar("##value", ImGuiDataType_U64, &v)) {
                    value = static_cast<uint64_t>(v);
                    return true;
                }
                return false;
            }
            if (value.is_number_float()) {
                double v = value.get<double>();
                if (read_only) {
                    ImGui::TextDisabled("%.6g", v);
                    return false;
                }
                if (ImGui::InputDouble("##value", &v, 0.1, 1.0, "%.6g")) {
                    value = v;
                    return true;
                }
                return false;
            }
            if (value.is_string()) {
                std::array<char, 1024> buffer{};
                const std::string current = value.get<std::string>();
                std::snprintf(buffer.data(), buffer.size(), "%s", current.c_str());
                if (read_only) {
                    ImGui::TextDisabled("%s", current.c_str());
                    return false;
                }
                if (ImGui::InputText("##value", buffer.data(), buffer.size())) {
                    value = std::string(buffer.data());
                    return true;
                }
                return false;
            }
            if (value.is_object()) {
                bool changed = false;
                for (auto& [name, child] : value.items()) {
                    changed |= DrawJsonMember(String(name.c_str()), child, read_only);
                }
                return changed;
            }
            if (value.is_array()) {
                bool changed = false;
                for (std::size_t i = 0; i < value.size(); ++i) {
                    ImGui::PushID(static_cast<int>(i));
                    if (value[i].is_object() || value[i].is_array()) {
                        const bool open = ImGui::TreeNodeEx("##array", ImGuiTreeNodeFlags_SpanAvailWidth,
                                                            "[%zu]", i);
                        if (open) {
                            changed |= DrawJsonValue(value[i], read_only);
                            ImGui::TreePop();
                        }
                    }
                    else {
                        ImGui::Text("[%zu]", i);
                        ImGui::SameLine();
                        changed |= DrawJsonValue(value[i], read_only);
                    }
                    ImGui::PopID();
                }
                return changed;
            }

            ImGui::TextDisabled("null");
            return false;
        }

        void DrawNativeComponent(Entity& entity, const ComponentDB::Entry& entry) {
            ImGui::PushID(static_cast<int>(entry.type));
            if (!ImGui::CollapsingHeader(entry.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::PopID();
                return;
            }

            void* component = entry.get(entity);
            if (!component || !entry.writeJson) {
                ImGui::TextDisabled("No editable fields");
                ImGui::PopID();
                return;
            }

            Json fields = entry.writeJson(component);
            const bool changed = DrawJsonValue(fields, entry.name == "IDComponent");
            if (changed && entry.readJson && entry.readJson(component, fields) && entry.markDirty) {
                entry.markDirty(entity);
            }
            ImGui::PopID();
        }

        void DrawManagedComponents(Entity entity) {
            auto* script_system = GetScriptSystem();
            auto* runtime = script_system ? script_system->getScriptRuntime() : nullptr;
            if (!runtime) return;

            DynamicArray<Pair<String, Json>> components;
            if (!runtime->getEntityManagedComponentFields(static_cast<uint64_t>(entity.uuid()), components)) {
                return;
            }

            for (auto& [type_name, fields] : components) {
                ImGui::PushID(type_name.c_str());
                String title = type_name;
                const auto dot = title.find_last_of('.');
                if (dot != String::npos) {
                    title = title.substr(dot + 1);
                }
                if (ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                    if (DrawJsonValue(fields, false)) {
                        runtime->setEntityManagedComponentFields(
                            static_cast<uint64_t>(entity.uuid()), type_name, fields);
                    }
                }
                ImGui::PopID();
            }
        }
    }

    void DebugImGui::RegisterDebugPanel() {
        if (s_registered) return;
        GetDebugger()->addImGuiRenderFunc("DebugImGui", OnImGuiRender);
        s_registered = true;
    }

    void DebugImGui::UnregisterDebugPanel() {
        if (!s_registered) return;
        GetDebugger()->removeImGuiRenderFunc("DebugImGui");
        s_registered = false;
    }

    void DebugImGui::OnImGuiRender() {
#ifndef DODOE_EDITOR_ENABLED
        RenderHierarchyPanel();
        RenderInspectorPanel();
        RenderDebuggerPanel();
#endif//DODOE_EDITOR_ENABLED;
    }

    void DebugImGui::RenderDebuggerPanel() { 
        ImGui::Begin("Dodoe Debugger");
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("FPS: %.1f (%.3f ms)", io.Framerate, 1000.0f / io.Framerate);

        if (ImGui::Button("Switch World State")) {
            GetWorld()->switchState();
        }

        if (ImGui::Button("Reload Scripts")) {
            Bool success = GetScriptSystem()->reloadScripts();
            if (success) DO_INFO("Reload Scripts succeed!");
        }
        ImGui::End();
    }

    void DebugImGui::RenderHierarchyPanel() {
        ImGui::Begin("Hierarchy");
        Scene* scene = GetWorld()->getActiveScene();
        if (!scene) {
            ImGui::TextUnformatted("No active scene.");
            ImGui::End();
            return;
        }
        for (const EntityNode& root : BuildEntityTree(*scene)) RenderEntityTreeNode(root);
        ImGui::End();
    }

    DynamicArray<DebugImGui::EntityNode> DebugImGui::BuildEntityTree(Scene& scene) {
        const auto all_entities = scene.getEntities();
        UnorderedMap<UUID, Entity> by_uuid;
        UnorderedMap<UUID, DynamicArray<UUID>> children;
        DynamicArray<UUID> root_ids;
        by_uuid.reserve(all_entities.size());

        for (Entity entity : all_entities) by_uuid.emplace(entity.uuid(), entity);
        for (Entity entity : all_entities) {
            UUID parent_uuid{};
            Entity parent{};
            if (entity.hasComponent<HierarchyComponent>()) {
                parent = entity.getComponent<HierarchyComponent>().parent;
            }
            const bool has_parent = parent.valid() && scene.registry().valid(parent);
            if (has_parent) parent_uuid = parent.uuid();
            if (!has_parent || parent_uuid == entity.uuid() || !by_uuid.contains(parent_uuid)) {
                root_ids.push_back(entity.uuid());
            }
            else {
                children[parent_uuid].push_back(entity.uuid());
            }
        }

        DynamicArray<EntityNode> roots;
        UUIDSet built;
        std::function<EntityNode(const UUID&)> make_node = [&](const UUID& uuid) {
            EntityNode node{by_uuid.at(uuid), {}};
            built.insert(uuid);
            auto it = children.find(uuid);
            if (it != children.end()) {
                for (const UUID& child_uuid : it->second) {
                    if (!built.contains(child_uuid)) node.children.push_back(make_node(child_uuid));
                }
            }
            return node;
        };
        for (const UUID& uuid : root_ids) {
            if (!built.contains(uuid)) roots.push_back(make_node(uuid));
        }
        for (const auto& [uuid, _] : by_uuid) {
            if (!built.contains(uuid)) roots.push_back(make_node(uuid));
        }
        return roots;
    }

    void DebugImGui::RenderEntityTreeNode(const EntityNode& node) {
        Entity entity = node.entity;
        if (!entity.valid()) return;

        ImGui::PushID(static_cast<int>(static_cast<ui32>(entity)));
        const String& name = entity.name();
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (node.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
        if (s_selectedEntity.valid() && entity.uuid() == s_selectedEntity.uuid()) flags |= ImGuiTreeNodeFlags_Selected;

        const bool opened = ImGui::TreeNodeEx("Entity", flags, "%s", name.c_str());
        if (ImGui::IsItemClicked()) s_selectedEntity = entity;
        if (opened) {
            for (const EntityNode& child : node.children) RenderEntityTreeNode(child);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    void DebugImGui::RenderInspectorPanel() {
        ImGui::Begin("Inspector");
        if (!s_selectedEntity.valid()) {
            ImGui::TextUnformatted("No entity selected.");
            ImGui::End();
            return;
        }

        Entity entity = s_selectedEntity;
        auto& db = ComponentDB::self();
        for (const auto& entry : db.entries()) {
            if (entry.contains(entity)) DrawNativeComponent(entity, entry);
        }
        DrawManagedComponents(entity);
        ImGui::End();
    }

}

#endif
