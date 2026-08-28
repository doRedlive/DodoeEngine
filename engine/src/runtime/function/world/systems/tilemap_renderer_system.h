// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/world/systems/system.h"
#include "runtime/function/world/components.h"

namespace dodoe {

    class TilemapRendererSystem : public System {
        UnorderedMap<UUID, DynamicArray<UUID>> m_submitted_chunks{};
        UnorderedMap<UUID, UInt64> m_tilemap_signatures{};

    public:
        ~TilemapRendererSystem() override;

        [[nodiscard]] SystemAccess getAccess() const override;
        void update(Registry& reg, float dt) override;

    private:
        void syncTilemap(Entity entity);
        void pruneRemovedChunks(const UnorderedSet<UUID>& active_chunks);
        static UUID MakeChunkUuid(UUID tilemap_uuid, Size_t layer_index, UInt32 chunk_x, UInt32 chunk_y);
        static UInt64 ComputeTilemapSignature(Entity entity);
    };

} // dodoe
