// do@Redlive

#pragma once

#include "dopch.h"

#include "system.h"
#include "../components.h"

namespace dodoe {

    class SkyLightSystem : public System {
        UnorderedSet<UUID> m_submitted{};

    public:
        ~SkyLightSystem() override;
        [[nodiscard]] SystemAccess getAccess() const override;
        void update(Registry& reg, float dt) override;

    private:
        bool syncSkyLight(Entity entity);
        void pruneRemoved(const UnorderedSet<UUID>& active);
        TextureCubemap* loadCubemap(const DynamicArray<String>& paths);
    };

} // dodoe
