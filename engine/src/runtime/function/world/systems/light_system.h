// do@Redlive
#pragma once

#include "dopch.h"

#include "system.h"
#include "../components.h"

namespace dodoe {

    class RenderScene;

    class LightSystem : public System {
        enum class LightKind {
            Point,
            Spot,
        };

        std::unordered_map<Uuid, LightKind> m_submitted_lights;

    public:
        ~LightSystem() override;

        void update(Registry& reg, float dt) override;

    private:
        bool syncPointLight(RenderScene& render_scene, Entity entity, PointLightComponent& light);
        bool syncSpotLight(RenderScene& render_scene, Entity entity, SpotLightComponent& light);
        bool pruneRemovedLights(RenderScene& render_scene, const std::unordered_set<Uuid>& alive_nodes);
        [[nodiscard]] bool needsLightSync(const RenderScene& render_scene, Entity entity, LightKind kind, bool light_dirty) const;
        [[nodiscard]] static Uuid resolveParentUuid(Entity entity);
    };

} // dodoe
