// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/function/world/systems/system.h"
#include "runtime/function/world/components.h"

namespace dodoe {

    struct TileKey {
        UUID tilemap_uuid{};
        Size_t layer_index{0};
        Int32 tile_x{0};
        Int32 tile_y{0};

        bool operator==(const TileKey& other) const {
            return tilemap_uuid == other.tilemap_uuid
                && layer_index == other.layer_index
                && tile_x == other.tile_x
                && tile_y == other.tile_y;
        }
    };

    struct TileKeyHash {
        Size_t operator()(const TileKey& k) const {
            Size_t h = static_cast<Size_t>(static_cast<uint64_t>(k.tilemap_uuid));
            h ^= k.layer_index + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= static_cast<Size_t>(k.tile_x) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= static_cast<Size_t>(k.tile_y) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    class TilemapRendererSystem : public System {
        UnorderedMap<TileKey, UUID, TileKeyHash> m_submitted_tiles{};

    public:
        ~TilemapRendererSystem() override;

        [[nodiscard]] SystemAccess getAccess() const override;
        void update(Registry& reg, float dt) override;

    private:
        void syncTilemap(Entity entity);
        void pruneRemovedTiles(const UnorderedSet<UUID>& active_tiles);
        static UUID MakeTileUuid(UUID tilemap_uuid, Size_t layer_index, Int32 tx, Int32 ty);
        static Matrix4f BuildTileWorldMatrix(Float pos_x, Float pos_y, Float width, Float height);
    };

} // dodoe
