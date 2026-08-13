// do@Redlive

#pragma once

#include "dopch.h"

#include "asset.h"
#include "runtime/core/object/object_id.h"

namespace dodoe {

    class AssetDatabase {
        FsPath m_database_path;
        UnorderedMap<ObjectID, AssetMetaData> m_metadata_cache;
        Bool m_dirty{false};

    public:
        explicit AssetDatabase(const FsPath& project_asset_dir);
        ~AssetDatabase();

        [[nodiscard]] Bool load();
        Bool save();

        [[nodiscard]] Bool hasAsset(const ObjectID& id) const;
        [[nodiscard]] AssetMetaData getMetaData(const ObjectID& id) const;
        void setMetaData(const ObjectID& id, const AssetMetaData& meta);
        void removeAsset(const ObjectID& id);

        [[nodiscard]] DynamicArray<ObjectID> getAllAssetIDs() const;
        [[nodiscard]] DynamicArray<ObjectID> getAssetsOfType(AssetType type) const;
        [[nodiscard]] DynamicArray<ObjectID> getAssetsByTag(const String& tag) const;

        void markDirty() { m_dirty = true; }
        [[nodiscard]] Bool isDirty() const { return m_dirty; }
        [[nodiscard]] static UUID generateUUID();
    };

} // dodoe
