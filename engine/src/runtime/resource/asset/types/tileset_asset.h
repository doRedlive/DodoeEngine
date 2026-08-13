// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/resource/asset/asset.h"

namespace dodoe {

    class TilesetAsset : public Asset {
        String m_name{};
        UInt32 m_first_gid{1};
        UInt32 m_tile_width{16};
        UInt32 m_tile_height{16};
        UInt32 m_columns{0};
        UInt32 m_tile_count{0};
        String m_image_path{};
        Identifier m_texture_id{0};

    public:
        static constexpr AssetType kStaticType = AssetType::Tileset;

        TilesetAsset() { m_meta.type = AssetType::Tileset; }

        [[nodiscard]] Bool loadFromSource(const String& absolute_source_path) override;
        void unloadRuntime() override;
        [[nodiscard]] Bool isReadOnly() const override { return false; }
        [[nodiscard]] Bool saveToSource(const String& absolute_path) const override;

        [[nodiscard]] const String& getName() const { return m_name; }
        [[nodiscard]] UInt32 getFirstGid() const { return m_first_gid; }
        [[nodiscard]] UInt32 getTileWidth() const { return m_tile_width; }
        [[nodiscard]] UInt32 getTileHeight() const { return m_tile_height; }
        [[nodiscard]] UInt32 getColumns() const { return m_columns; }
        [[nodiscard]] UInt32 getTileCount() const { return m_tile_count; }
        [[nodiscard]] const String& getImagePath() const { return m_image_path; }
        [[nodiscard]] Identifier getTextureId() const { return m_texture_id; }
    };

} // dodoe
