// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/context/system_context.h"
#include "runtime/core/debug/debugger.h"
#include "runtime/function/world/entity.h"

namespace dodoe {

    class DebugImGui {
    public:
        static void RegisterDebugPanel();
        static void UnregisterDebugPanel();

    private:
        static void OnImGuiRender();

        static void RenderHierarchyPanel();

        struct EntityNode {
            Entity entity;
            std::vector<EntityNode> children;
        };
        static std::vector<EntityNode> BuildEntityTree(Scene& scene);
        static void RenderEntityTreeNode(const EntityNode& node);

        static void RenderInspectorPanel();

        static void DrawComponentHeader(const std::string& label);
        static void InspectIDComponent(Entity entity);
        static void InspectTagComponent(Entity entity);
        static void InspectTransformComponent(Entity entity);
        static void InspectHierarchyComponent(Entity entity);
        static void InspectCamera2dComponent(Entity entity);
        static void InspectSpriteRendererComponent(Entity entity);
        static void InspectRigidbody2dComponent(Entity entity);

        static inline bool  s_registered = false;
        static inline Entity s_selectedEntity{};
    };

} // dodoe
