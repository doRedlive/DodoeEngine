// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/context/system_context.h"
#include "runtime/core/debug/debugger.h"
#include "runtime/function/world/entity.h"

#include <atomic>

#if defined(DODOE_DEBUG_ENABLED) && defined(DODOE_IMGUI_ENABLED)

namespace dodoe {

    class DebugImGui {
    public:
        static void RegisterDebugPanel();
        static void UnregisterDebugPanel();

        // Entity currently selected in the debug hierarchy panel (invalid when nothing is selected).
        static Entity GetSelectedEntity() { return s_selectedEntity; }

        static void RequestViewportPick(Int32 x, Int32 y);
        static Bool ConsumePickRequest(Int32& out_x, Int32& out_y);
        static void SubmitPickResult(UInt64 entity_uuid);
        static Bool ConsumePickResult(UInt64& out_entity_uuid);

    private:
        static void OnImGuiRender();

        static void ApplyPickedEntity(UInt64 entity_uuid);

        static void RenderHierarchyPanel();
        static void RenderInspectorPanel();
        static void RenderEntityMaterials(Entity entity);
        static void RenderDebuggerPanel();
        static void RenderToolActions();

        struct EntityNode {
            Entity entity;
            DynamicArray<EntityNode> children;
        };
        static DynamicArray<EntityNode> BuildEntityTree(Scene& scene);
        static void RenderEntityTreeNode(const EntityNode& node);
        static bool ValidateSelectedEntity(Scene& scene);


        static inline bool  s_registered = false;
        static inline Entity s_selectedEntity{};
        static inline UUID s_material_owner_entity{};
        static inline bool s_material_owner_valid = false;
        static inline DynamicArray<Entity> s_material_owner_candidates{};
        static inline std::atomic<UInt32> s_pick_requested{0};
        static inline std::atomic<Int32> s_pick_x{0};
        static inline std::atomic<Int32> s_pick_y{0};
        static inline std::atomic<UInt64> s_pick_result{0};
        static inline std::atomic<UInt32> s_pick_result_valid{0};
    };

} // dodoe

#endif//DODOE_DEBUG_ENABLED && DODOE_IMGUI_ENABLED
