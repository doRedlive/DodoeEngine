// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/function/render/pixel2d/tileset.h"
#include "runtime/resource/asset/asset.h"

REFLECTION_TYPE(TilesetAsset)

namespace dodoe {

    CLASS(TilesetAsset, WhiteListFields) : public Asset {
        REFLECTION_BODY(TilesetAsset)

        META(Enable)
        String m_name{};
        META(Enable)
        UInt32 m_first_gid{1};
        META(Enable)
        UInt32 m_tile_width{16};
        META(Enable)
        UInt32 m_tile_height{16};
        META(Enable)
        UInt32 m_columns{0};
        META(Enable)
        UInt32 m_tile_count{0};
        META(Enable)
        UInt32 m_margin{0};
        META(Enable)
        UInt32 m_spacing{0};
        META(Enable)
        String m_image_path{};
        Identifier m_texture_id{0};
        TilePropertyMap m_tile_properties{};

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
        [[nodiscard]] UInt32 getMargin() const { return m_margin; }
        [[nodiscard]] UInt32 getSpacing() const { return m_spacing; }
        [[nodiscard]] const String& getImagePath() const { return m_image_path; }
        [[nodiscard]] Identifier getTextureId() const { return m_texture_id; }
        [[nodiscard]] const TilePropertyMap& getTileProperties() const { return m_tile_properties; }

        void updateGrid(UInt32 tileWidth, UInt32 tileHeight, UInt32 columns, UInt32 tileCount,
                        UInt32 margin, UInt32 spacing) {
            m_tile_width = tileWidth;
            m_tile_height = tileHeight;
            m_columns = columns;
            m_tile_count = tileCount;
            m_margin = margin;
            m_spacing = spacing;
        }
    };

} // dodoe
