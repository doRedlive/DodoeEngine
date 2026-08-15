// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/resource/asset/asset.h"
#include "runtime/resource/parser/texture_blob.h"
#include "runtime/function/render/pixel2d/sprite.h"

namespace dodoe {

    class Texture2D;

    class TextureAsset : public Asset {
        TextureBlob m_blob{};
        Float m_ppu{kDefaultPixelsPerUnit};
        Bool m_flip_vertical{true};

    public:
        static constexpr AssetType kStaticType = AssetType::Texture;

        TextureAsset() { m_meta.type = AssetType::Texture; }

        [[nodiscard]] Bool loadFromSource(const String& absolute_source_path) override;
        void unloadRuntime() override;
        [[nodiscard]] Bool isReadOnly() const override { return true; }

        [[nodiscard]] const TextureBlob& getBlob() const { return m_blob; }
        [[nodiscard]] Float getPPU() const { return m_ppu; }
        [[nodiscard]] Bool getFlipVertical() const { return m_flip_vertical; }
        [[nodiscard]] Int32 getWidth() const { return m_blob.width; }
        [[nodiscard]] Int32 getHeight() const { return m_blob.height; }
        [[nodiscard]] Int32 getChannels() const { return m_blob.channels; }

        void setPPU(Float ppu) { m_ppu = ppu; }
        void setFlipVertical(Bool flip) { m_flip_vertical = flip; }
    };

} // dodoe
