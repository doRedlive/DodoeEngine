// do@Redlive

#pragma once

#include "dopch.h"

#include "system.h"
#include "../components.h"
#include "runtime/function/render/render_scene/primitive_render_object.h"

namespace dodoe {

    class MeshRendererSystem : public System {
        UnorderedSet<UUID> m_submitted_objects{};
        UnorderedMap<UUID, UInt32> m_skinning_slots{};
        UInt32 m_next_skinning_slot{0};
        DynamicArray<Matrix4f> m_skinning_staging{};

    public:
        ~MeshRendererSystem() override;

        [[nodiscard]] SystemAccess getAccess() const override;
        void update(Registry& reg, float dt) override;

    private:
        bool syncRenderObject(Entity entity);
        void pruneRemovedObjects(const UnorderedSet<UUID>& active_renderers);
        void propagateHierarchyDirty(Registry& reg);
        [[nodiscard]] UInt32 acquireSkinningSlot(const UUID& id);
        void copySkinningToStaging(const UUID& id, const DynamicArray<Matrix4f>& matrices);
        static void markSubtreeTransformDirty(const std::vector<Entity>& children, std::size_t depth);

        [[nodiscard]] static bool needsRenderObjectSync(Entity entity, const UnorderedSet<UUID>& submitted);
        [[nodiscard]] static Matrix4f buildWorldMatrix(Entity entity);
        [[nodiscard]] static Matrix4f buildLocalMatrix(const TransformComponent& transform);
        [[nodiscard]] static Scope<PrimitiveRenderObject> buildRenderObject(const MeshRendererComponent& mesh, UInt32 skinning_offset);
    };

} // dodoe
