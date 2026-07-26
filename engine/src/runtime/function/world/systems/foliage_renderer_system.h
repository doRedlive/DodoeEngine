#pragma once

#include "dopch.h"

#include "system.h"
#include "../components.h"
#include "runtime/function/render/render_scene/primitive_render_object.h"

namespace dodoe {

    class FoliageRendererSystem : public System {
        UnorderedSet<UUID> m_submitted_objects{};

    public:
        ~FoliageRendererSystem() override;

        void update(Registry& reg, float dt) override;

    private:
        bool syncFoliageRenderer(Entity entity);
        void pruneRemovedObjects(const UnorderedSet<UUID>& active_objects);

        [[nodiscard]] bool needsObjectSync(Entity entity) const;
        [[nodiscard]] static Matrix4f buildWorldMatrix(const TransformComponent& transform);
        [[nodiscard]] static Scope<PrimitiveRenderObject> buildRenderObject(const FoliageRendererComponent& component);
    };

} // dodoe
