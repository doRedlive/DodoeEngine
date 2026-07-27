// do@Redlive

#pragma once

#include "dopch.h"

#include "system.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"

namespace dodoe {

    class SpriteRendererSystem : public System {
        UnorderedSet<UUID> m_submitted_sprites{};

    public:
        ~SpriteRendererSystem() override;

        [[nodiscard]] SystemAccess getAccess() const override;

        void update(Registry& reg, float dt) override;

    private:
        bool syncSpriteRenderer(Entity entity);
        void pruneRemovedSprites(const UnorderedSet<UUID>& active_sprites);
        bool needsSync(Entity entity) const;
        Matrix4f buildWorldMatrix(const TransformComponent& transform);
    };

} // dodoe
