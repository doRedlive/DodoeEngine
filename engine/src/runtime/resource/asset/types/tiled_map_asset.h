// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/resource/asset/asset.h"

namespace dodoe {

    struct TiledMapTilesetData {
        UInt32 first_gid{1};
        String name{};
        UInt32 tile_width{0};
        UInt32 tile_height{0};
        UInt32 columns{0};
        UInt32 tile_count{0};
        UInt32 margin{0};
        UInt32 spacing{0};
        String image_path{};
    };

    struct TiledMapLayerData {
        String name{};
        UInt32 width{0};
        UInt32 height{0};
        Bool visible{true};
        Float opacity{1.0f};
        Int32 offset_x{0};
        Int32 offset_y{0};
        DynamicArray<UInt32> tiles;
    };

    class TiledMapAsset : public Asset {
        UInt32 m_map_width{0};
        UInt32 m_map_height{0};
        UInt32 m_tile_width{16};
        UInt32 m_tile_height{16};
        String m_orientation{"orthogonal"};
        DynamicArray<TiledMapTilesetData> m_tilesets;
        DynamicArray<TiledMapLayerData> m_layers;

    public:
        static constexpr AssetType kStaticType = AssetType::TiledMap;

        TiledMapAsset() { m_meta.type = AssetType::TiledMap; }

        [[nodiscard]] Bool loadFromSource(const String& absolute_source_path) override;
        void unloadRuntime() override;
        [[nodiscard]] Bool isReadOnly() const override { return true; }

        [[nodiscard]] UInt32 getMapWidth() const { return m_map_width; }
        [[nodiscard]] UInt32 getMapHeight() const { return m_map_height; }
        [[nodiscard]] UInt32 getTileWidth() const { return m_tile_width; }
        [[nodiscard]] UInt32 getTileHeight() const { return m_tile_height; }
        [[nodiscard]] const String& getOrientation() const { return m_orientation; }
        [[nodiscard]] const DynamicArray<TiledMapTilesetData>& getTilesets() const { return m_tilesets; }
        [[nodiscard]] const DynamicArray<TiledMapLayerData>& getLayers() const { return m_layers; }
    };

} // dodoe
