// do@Redlive

#pragma once

#include "dopch.h"

#include "system.h"
#include "../components.h"
#include "runtime/function/render/render_scene/primitive_render_object.h"

namespace dodoe {

    class MeshRendererSystem : public System {
        UnorderedSet<UUID> m_submitted_objects{};

    public:
        ~MeshRendererSystem() override;

        [[nodiscard]] SystemAccess getAccess() const override;
        void update(Registry& reg, float dt) override;

    private:
        bool syncRenderObject(Entity entity);
        void pruneRemovedObjects(const UnorderedSet<UUID>& active_renderers);

        [[nodiscard]] static bool needsRenderObjectSync(Entity entity, const UnorderedSet<UUID>& submitted);
        [[nodiscard]] static Matrix4f buildWorldMatrix(const TransformComponent& transform);
        [[nodiscard]] static Scope<PrimitiveRenderObject> buildRenderObject(const MeshRendererComponent& mesh);
    };

} // dodoe
