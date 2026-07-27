// do@Redlive
#pragma once

#include "dopch.h"

#include "system.h"
#include "../components.h"
#include "runtime/function/render/render_scene/light_scene_info.h"

namespace dodoe {

    class LightSystem : public System {
        std::unordered_map<UUID, LightType> m_submitted_lights;

    public:
        ~LightSystem() override;

        [[nodiscard]] SystemAccess getAccess() const override;
        void update(Registry& reg, float dt) override;

    private:
        bool syncPointLight(Entity entity);
        bool syncSpotLight(Entity entity);
        void pruneRemovedLights(const UnorderedSet<UUID>& active_lights);

        [[nodiscard]] bool needsLightSync(Entity entity, LightType kind) const;
        [[nodiscard]] static Matrix4f buildWorldMatrix(const TransformComponent& transform);
    };

} // dodoe
