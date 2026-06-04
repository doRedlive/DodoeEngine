// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/resource/asset/asset.h"
#include "runtime/resource/parser/texture_blob.h"

namespace dodoe {

    class Texture;

    class TextureAsset : public Asset {
        TextureBlob m_blob{};
        Ref<Texture> m_gpu_texture{};
        Float m_ppu{100.0f};
        Bool m_flip_vertical{true};

    public:
        static constexpr AssetType kStaticType = AssetType::Texture;

        TextureAsset() { m_meta.type = AssetType::Texture; }

        [[nodiscard]] Bool loadFromSource(const String& absolute_source_path) override;
        void unloadRuntime() override;
        [[nodiscard]] Bool isReadOnly() const override { return true; }

        [[nodiscard]] const TextureBlob& getBlob() const { return m_blob; }
        [[nodiscard]] Ref<Texture> getGPUTexture() const { return m_gpu_texture; }
        [[nodiscard]] Float getPPU() const { return m_ppu; }
        [[nodiscard]] Bool getFlipVertical() const { return m_flip_vertical; }
        [[nodiscard]] Int32 getWidth() const { return m_blob.width; }
        [[nodiscard]] Int32 getHeight() const { return m_blob.height; }
        [[nodiscard]] Int32 getChannels() const { return m_blob.channels; }
    };

} // dodoe
