// do@Redlive

#pragma once

#include "dopch.h"

#include "system.h"
#include "../components.h"

namespace dodoe {

    class AudioPlaySystem : public System {
        UnorderedMap<UUID, AudioSourceId> m_sources{};
        UnorderedSet<UUID> m_awake_played{};

    public:
        ~AudioPlaySystem() override;
        [[nodiscard]] SystemAccess getAccess() const override;
        void update(Registry& reg, float dt) override;

    private:
        void syncListener(Registry& reg);
        void syncSources(Registry& reg);
        void pruneRemoved(const UnorderedSet<UUID>& active);
    };

} // dodoe
