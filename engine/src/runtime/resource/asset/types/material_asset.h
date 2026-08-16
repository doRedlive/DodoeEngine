// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/resource/asset/asset.h"
#include "runtime/resource/asset/asset_handle.h"
#include "runtime/resource/asset/types/texture_asset.h"

REFLECTION_TYPE(MaterialAsset)

namespace dodoe {

    CLASS(MaterialAsset, WhiteListFields) : public Asset {
        REFLECTION_BODY(MaterialAsset)

        META(Enable)
        Vector4f m_color{1.0f, 1.0f, 1.0f, 1.0f};
        META(Enable)
        Vector3f m_emissive{0.0f, 0.0f, 0.0f};
        META(Enable)
        Float m_metallic{0.0f};
        META(Enable)
        Float m_roughness{1.0f};
        META(Enable, AssetHandle)
        AssetHandle<TextureAsset> m_base_color_texture{};
        META(Enable, AssetHandle)
        AssetHandle<TextureAsset> m_normal_texture{};
        META(Enable, AssetHandle)
        AssetHandle<TextureAsset> m_metallic_roughness_texture{};
        META(Enable, AssetHandle)
        AssetHandle<TextureAsset> m_emissive_texture{};

    public:
        static constexpr AssetType kStaticType = AssetType::Material;

        MaterialAsset() { m_meta.type = AssetType::Material; }

        [[nodiscard]] Bool loadFromSource(const String& absolute_source_path) override;
        void unloadRuntime() override;
        [[nodiscard]] Bool isReadOnly() const override { return false; }
        [[nodiscard]] Bool saveToSource(const String& absolute_path) const override;

        [[nodiscard]] const Vector4f& getColor() const { return m_color; }
        [[nodiscard]] const Vector3f& getEmissive() const { return m_emissive; }
        [[nodiscard]] Float getMetallic() const { return m_metallic; }
        [[nodiscard]] Float getRoughness() const { return m_roughness; }
        [[nodiscard]] AssetHandle<TextureAsset> getBaseColorTexture() const { return m_base_color_texture; }
        [[nodiscard]] AssetHandle<TextureAsset> getNormalTexture() const { return m_normal_texture; }
        [[nodiscard]] AssetHandle<TextureAsset> getMetallicRoughnessTexture() const { return m_metallic_roughness_texture; }
        [[nodiscard]] AssetHandle<TextureAsset> getEmissiveTexture() const { return m_emissive_texture; }
    };

} // dodoe
