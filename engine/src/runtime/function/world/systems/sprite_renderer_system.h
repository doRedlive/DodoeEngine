// do@Redlive

#pragma once

#include "dopch.h"

#include "system.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components.h"

namespace dodoe {

    class SpriteRendererSystem : public System {
        std::unordered_set<UUID> m_submitted_sprites{};

    public:
        ~SpriteRendererSystem() override;

        void update(Registry& reg, float dt) override;

    private:
        bool syncSpriteRenderer(Entity entity);
        void pruneRemovedSprites(const std::unordered_set<UUID>& active_sprites);
        bool needsSync(Entity entity) const;
        Matrix4f buildWorldMatrix(const TransformComponent& transform);
    };

} // dodoe
