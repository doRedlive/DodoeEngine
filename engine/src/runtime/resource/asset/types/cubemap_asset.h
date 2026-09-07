// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/resource/asset/asset.h"

namespace dodoe {

    class CubemapAsset : public Asset {
        DynamicArray<String> m_face_paths{};

    public:
        static constexpr AssetType kStaticType = AssetType::Cubemap;

        CubemapAsset() { m_meta.type = AssetType::Cubemap; }

        [[nodiscard]] Bool loadFromSource(const String& absolute_source_path) override;
        void unloadRuntime() override;
        [[nodiscard]] Bool isReadOnly() const override { return false; }
        [[nodiscard]] Bool saveToSource(const String& absolute_path) const override;

        [[nodiscard]] const DynamicArray<String>& getFacePaths() const { return m_face_paths; }
        void setFacePaths(const DynamicArray<String>& face_paths) { m_face_paths = face_paths; }
    };

} // dodoe
