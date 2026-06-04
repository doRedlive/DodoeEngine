// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/resource/asset/asset.h"
#include "runtime/resource/parser/mesh_blob.h"

namespace dodoe {

    class MeshAsset : public Asset {
        MeshBlob m_blob{};

    public:
        static constexpr AssetType kStaticType = AssetType::Mesh;

        MeshAsset() { m_meta.type = AssetType::Mesh; }

        [[nodiscard]] Bool loadFromSource(const String& absolute_source_path) override;
        void unloadRuntime() override;
        [[nodiscard]] Bool isReadOnly() const override { return true; }

        [[nodiscard]] const MeshBlob& getBlob() const { return m_blob; }
        [[nodiscard]] Ref<MeshData> getData() const { return m_blob.data; }
    };

} // dodoe
