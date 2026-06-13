#pragma once

#include "dopch.h"

#include "system.h"
#include "../components.h"

namespace dodoe {

    class RenderObject;
    class RenderScene;

    class FoliageRendererSystem : public System {
        std::unordered_set<Uuid> m_submitted_objects{};

    public:
        ~FoliageRendererSystem() override;

        void update(Registry& reg, float dt) override;

    private:
        bool syncFoliageRenderer(RenderScene& render_scene, Entity entity);
        bool pruneRemovedObjects(RenderScene& render_scene, const std::unordered_set<Uuid>& alive_objects);

        [[nodiscard]] bool needsObjectSync(const RenderScene& render_scene, Entity entity) const;
        [[nodiscard]] static Scope<RenderObject> buildRenderObject(const FoliageRendererComponent& component);
    };

} // dodoe
