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

        // Entity currently selected in the debug hierarchy panel (invalid when nothing is selected).
        static Entity GetSelectedEntity() { return s_selectedEntity; }

    private:
        static void OnImGuiRender();

        static void RenderHierarchyPanel();
        static void RenderInspectorPanel();
        static void RenderDebuggerPanel();
        static void RenderToolActions();

        struct EntityNode {
            Entity entity;
            DynamicArray<EntityNode> children;
        };
        static DynamicArray<EntityNode> BuildEntityTree(Scene& scene);
        static void RenderEntityTreeNode(const EntityNode& node);


        static inline bool  s_registered = false;
        static inline Entity s_selectedEntity{};
    };

} // dodoe
