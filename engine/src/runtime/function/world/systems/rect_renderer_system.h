// do@Redlive

#pragma once

#include "dopch.h"

#include "system.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"

namespace dodoe {

    class RectRendererSystem : public System {
        UnorderedSet<UUID> m_submitted{};

    public:
        ~RectRendererSystem() override;

        [[nodiscard]] SystemAccess getAccess() const override;
        void update(Registry& reg, Float dt) override;

    private:
        Bool syncRect(Entity entity);
        void pruneRemoved(const UnorderedSet<UUID>& active);
        Bool needsSync(Entity entity) const;
        Matrix4f buildWorldMatrix(const TransformComponent& transform, const RectRendererComponent& rect) const;
    };

} // dodoe
