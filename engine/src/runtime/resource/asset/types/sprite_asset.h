// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/resource/asset/asset.h"
#include "runtime/resource/file/file_id.h"

namespace dodoe {

    struct Rect2f {
        Float left{0.0f};
        Float bottom{0.0f};
        Float right{0.0f};
        Float top{0.0f};
    };

    class SpriteAsset : public Asset {
        FileID m_texture_source{};
        Float m_pixels_per_unit{100.0f};
        Vector2f m_pivot{0.5f, 0.5f};
        Rect2f m_slice{};

    public:
        static constexpr AssetType kStaticType = AssetType::Sprite;

        SpriteAsset() { m_meta.type = AssetType::Sprite; }

        [[nodiscard]] Bool loadFromSource(const String& absolute_source_path) override;
        void unloadRuntime() override;
        [[nodiscard]] Bool isReadOnly() const override { return true; }

        void setTextureSource(const FileID& texture_source) { m_texture_source = texture_source; }
        void setPixelsPerUnit(Float ppu) { m_pixels_per_unit = ppu; }
        void setPivot(const Vector2f& pivot) { m_pivot = pivot; }
        void setSlice(const Rect2f& slice) { m_slice = slice; }

        [[nodiscard]] const FileID& getTextureSource() const { return m_texture_source; }
        [[nodiscard]] Float getPixelsPerUnit() const { return m_pixels_per_unit; }
        [[nodiscard]] const Vector2f& getPivot() const { return m_pivot; }
        [[nodiscard]] const Rect2f& getSlice() const { return m_slice; }
    };

} // dodoe
