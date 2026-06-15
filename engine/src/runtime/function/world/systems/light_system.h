// do@Redlive
#pragma once

#include "dopch.h"

#include "system.h"
#include "../components.h"

namespace dodoe {

    class LightSystem : public System {
        enum class LightKind {
            Point,
            Spot,
        };

        std::unordered_map<UUID, LightKind> m_submitted_lights;

    public:
        ~LightSystem() override;

        void update(Registry& reg, float dt) override;

    private:
        bool syncPointLight(Entity entity);
        bool syncSpotLight(Entity entity);
        void pruneRemovedLights(const std::unordered_set<UUID>& active_lights);

        [[nodiscard]] bool needsLightSync(Entity entity, LightKind kind) const;
        [[nodiscard]] static Matrix4f buildWorldMatrix(const TransformComponent& transform);
    };

} // dodoe
