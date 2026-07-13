// do@Redlive

#include "debug_imgui.h"

#ifdef DODOE_DEBUG_ENABLED

#include "imgui/imgui.h"

#include "runtime/core/context/system_context.h"
#include "runtime/function/script/script_system.h"

#include "runtime/function/world/components/camera2d_component.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/world/components/id_component.h"
#include "runtime/function/world/components/rigidbody2d_component.h"
#include "runtime/function/world/components/sprite_renderer_component.h"
#include "runtime/function/world/components/tag_component.h"
#include "runtime/function/world/components/transform_component.h"

namespace dodoe {

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
        RenderHierarchyPanel();
        RenderInspectorPanel();
        RenderDebuggerPanel();
    }

    void DebugImGui::RenderDebuggerPanel() { 
        ImGui::Begin("Dodoe Debugger");
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Text("FPS: %.1f (%.3f ms)", io.Framerate, 1000.0f / io.Framerate);

        if (ImGui::Button("Reload Scripts")) {
            Bool success = GetScriptSystem()->reloadScripts();
            if (success) {
                DO_DEBUG("Reload Scripts success!");
                DO_DEBUG("MonoScriptSystem count is {}", GetScriptSystem()->getScriptRuntime()->logSystemClassCount());
            }


        }

        ImGui::End();
    }

    void DebugImGui::RenderHierarchyPanel() {
        ImGui::Begin("Hierarchy");

        Scene* scene = GetWorld()->getCurrentScene();
        if (!scene) {
            ImGui::TextUnformatted("No active scene.");
            ImGui::End();
            return;
        }

        std::vector<EntityNode> roots = BuildEntityTree(*scene);

        for (const EntityNode& root : roots) {
            RenderEntityTreeNode(root);
        }

        ImGui::End();
    }

    std::vector<DebugImGui::EntityNode> DebugImGui::BuildEntityTree(Scene& scene) {
        std::vector<EntityNode> roots;
        std::vector<Entity> all_entities = scene.getEntities();

        for (Entity entity : all_entities) {
            EntityNode node;
            node.entity = entity;

            if (!entity.hasComponent<HierarchyComponent>()) {
                roots.push_back(std::move(node));
                continue;
            }

            HierarchyComponent& hierarchy = entity.getComponent<HierarchyComponent>();
            if (!hierarchy.parent.valid()) {
                roots.push_back(std::move(node));
            }
        }

        for (Entity entity : all_entities) {
            if (!entity.hasComponent<HierarchyComponent>()) continue;

            HierarchyComponent& hierarchy = entity.getComponent<HierarchyComponent>();
            if (!hierarchy.parent.valid()) continue;

            Uuid parent_uuid = hierarchy.parent.uuid();

            auto find_parent = [&](EntityNode& node) -> EntityNode* {
                EntityNode* found = nullptr;
                std::function<void(EntityNode&)> search = [&](EntityNode& n) {
                    if (n.entity.uuid() == parent_uuid) {
                        found = &n;
                        return;
                    }
                    for (auto& child : n.children) {
                        search(child);
                    }
                };
                search(node);
                return found;
            };

            EntityNode* parent_node = nullptr;
            for (auto& root : roots) {
                parent_node = find_parent(root);
                if (parent_node) break;
            }

            if (parent_node) {
                EntityNode child_node;
                child_node.entity = entity;
                parent_node->children.push_back(std::move(child_node));
            } else {
                EntityNode orphan;
                orphan.entity = entity;
                roots.push_back(std::move(orphan));
            }
        }

        return roots;
    }

    void DebugImGui::RenderEntityTreeNode(const EntityNode& node) {
        Entity entity = node.entity;
        if (!entity.valid()) return;

        const std::string& name = entity.name();
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (node.children.empty()) {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }
        if (s_selectedEntity.valid() && entity.uuid() == s_selectedEntity.uuid()) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        bool opened = ImGui::TreeNodeEx(name.c_str(), flags);
        if (ImGui::IsItemClicked()) {
            s_selectedEntity = entity;
        }
        if (opened) {
            for (const EntityNode& child : node.children) {
                RenderEntityTreeNode(child);
            }
            ImGui::TreePop();
        }
    }

    void DebugImGui::RenderInspectorPanel() {
        ImGui::Begin("Inspector");

        if (!s_selectedEntity.valid()) {
            ImGui::TextUnformatted("No entity selected.");
            ImGui::End();
            return;
        }

        Entity entity = s_selectedEntity;

        if (entity.hasComponent<IDComponent>())        InspectIDComponent(entity);
        if (entity.hasComponent<TagComponent>())       InspectTagComponent(entity);
        if (entity.hasComponent<TransformComponent>()) InspectTransformComponent(entity);
        if (entity.hasComponent<HierarchyComponent>()) InspectHierarchyComponent(entity);
        if (entity.hasComponent<Camera2dComponent>())  InspectCamera2dComponent(entity);
        if (entity.hasComponent<SpriteRendererComponent>()) InspectSpriteRendererComponent(entity);
        if (entity.hasComponent<Rigidbody2dComponent>())   InspectRigidbody2dComponent(entity);

        ImGui::End();
    }

    void DebugImGui::InspectIDComponent(Entity entity) {
        if (!ImGui::CollapsingHeader("ID", ImGuiTreeNodeFlags_DefaultOpen)) return;

        IDComponent& idc = entity.getComponent<IDComponent>();
        char buffer[256];
        std::snprintf(buffer, sizeof(buffer), "%s", idc.name.c_str());
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::InputText("Name", buffer, sizeof(buffer))) {
            idc.setName(String(buffer));
        }
    }

    void DebugImGui::InspectTagComponent(Entity entity) {
        if (!ImGui::CollapsingHeader("Tag", ImGuiTreeNodeFlags_DefaultOpen)) return;

        TagComponent& tag = entity.getComponent<TagComponent>();
        char buffer[256];
        std::snprintf(buffer, sizeof(buffer), "%s", tag.tag.c_str());
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::InputText("Tag", buffer, sizeof(buffer))) {
            tag.setTag(String(buffer));
        }
    }

    void DebugImGui::InspectTransformComponent(Entity entity) {
        if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) return;

        TransformComponent& xform = entity.getComponent<TransformComponent>();

        float pos[3] = { xform.position.x, xform.position.y, xform.position.z };
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::DragFloat3("Position", pos, 0.1f)) {
            xform.setPosition(Vector3f(pos[0], pos[1], pos[2]));
        }

        float rot[3] = { xform.rotation.x, xform.rotation.y, xform.rotation.z };
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::DragFloat3("Rotation", rot, 0.1f)) {
            xform.setRotation(Vector3f(rot[0], rot[1], rot[2]));
        }

        float scl[3] = { xform.scale.x, xform.scale.y, xform.scale.z };
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::DragFloat3("Scale", scl, 0.1f, 0.01f, 100.0f)) {
            xform.setScale(Vector3f(scl[0], scl[1], scl[2]));
        }
    }

    void DebugImGui::InspectHierarchyComponent(Entity entity) {
        if (!ImGui::CollapsingHeader("Hierarchy", ImGuiTreeNodeFlags_DefaultOpen)) return;

        HierarchyComponent& hier = entity.getComponent<HierarchyComponent>();
        ImGui::Text("Child Count: %d", hier.child_count);
        if (hier.parent.valid()) {
            ImGui::Text("Parent: %s", hier.parent.name().c_str());
        } else {
            ImGui::TextUnformatted("Parent: None");
        }
    }

    void DebugImGui::InspectCamera2dComponent(Entity entity) {
        if (!ImGui::CollapsingHeader("Camera 2D", ImGuiTreeNodeFlags_DefaultOpen)) return;

        Camera2dComponent& cam = entity.getComponent<Camera2dComponent>();

        const char* type_names[] = { "None", "Perspective", "Orthographic" };
        int current_type = static_cast<int>(cam.type);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::Combo("Type", &current_type, type_names, IM_ARRAYSIZE(type_names))) {
            cam.setCameraType(static_cast<CameraType>(current_type));
        }

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::DragFloat("Zoom", &cam.zoom, 0.1f)) cam.dirty = true;

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::DragFloat("FOV", &cam.fov, 0.1f)) cam.dirty = true;

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::DragFloat("Near Plane", &cam.near_plane, 0.01f, 0.001f, cam.far_plane)) cam.dirty = true;

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::DragFloat("Far Plane", &cam.far_plane, 0.1f, cam.near_plane, 10000.0f)) cam.dirty = true;

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::DragFloat("Aspect Ratio", &cam.aspect_ratio, 0.01f)) cam.dirty = true;

        float bg[4] = { cam.background.r, cam.background.g, cam.background.b, cam.background.a };
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::ColorEdit4("Background", bg)) {
            cam.setBackgroundColor(Color(bg[0], bg[1], bg[2], bg[3]));
        }
    }

    void DebugImGui::InspectSpriteRendererComponent(Entity entity) {
        if (!ImGui::CollapsingHeader("Sprite Renderer", ImGuiTreeNodeFlags_DefaultOpen)) return;

        SpriteRendererComponent& sprite = entity.getComponent<SpriteRendererComponent>();

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::Checkbox("Flip", &sprite.flip)) sprite.dirty = true;

        float pivot[2] = { sprite.pivot.x, sprite.pivot.y };
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::DragFloat2("Pivot", pivot, 0.01f)) {
            sprite.pivot = Vector2f(pivot[0], pivot[1]);
            sprite.dirty = true;
        }

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::DragFloat("Depth", &sprite.depth, 0.01f)) sprite.dirty = true;

        float color[4] = { sprite.color.r, sprite.color.g, sprite.color.b, sprite.color.a };
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::ColorEdit4("Color", color)) {
            sprite.color = Color(color[0], color[1], color[2], color[3]);
            sprite.dirty = true;
        }
    }

    void DebugImGui::InspectRigidbody2dComponent(Entity entity) {
        if (!ImGui::CollapsingHeader("Rigidbody 2D", ImGuiTreeNodeFlags_DefaultOpen)) return;

        Rigidbody2dComponent& rb = entity.getComponent<Rigidbody2dComponent>();

        const char* type_names[] = { "Static", "Dynamic", "Kinematic" };
        int current_type = static_cast<int>(rb.type);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::Combo("Body Type", &current_type, type_names, IM_ARRAYSIZE(type_names))) {
            rb.type = static_cast<Rigidbody2dComponent::BodyType>(current_type);
        }

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::DragFloat("Gravity Scale", &rb.gravity_scale, 0.1f);

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::Checkbox("Fixed Rotation", &rb.fixed_rotation);
    }

} // dodoe

#endif // DODOE_DEBUG_ENABLED
