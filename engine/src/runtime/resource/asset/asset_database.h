// do@Redlive

#pragma once

#include "dopch.h"

#include "asset.h"

namespace dodoe {

    class AssetDatabase {
        FsPath m_database_path;
        UnorderedMap<FileID, AssetMetaData> m_metadata_cache;
        Bool m_dirty{false};

    public:
        explicit AssetDatabase(const FsPath& project_asset_dir);
        ~AssetDatabase();

        [[nodiscard]] Bool load();
        Bool save();

        [[nodiscard]] Bool hasAsset(const FileID& file_id) const;
        [[nodiscard]] AssetMetaData getMetaData(const FileID& file_id) const;
        void setMetaData(const FileID& file_id, const AssetMetaData& meta);
        void removeAsset(const FileID& file_id);

        [[nodiscard]] DynamicArray<FileID> getAllAssetFileIDs() const;
        [[nodiscard]] DynamicArray<FileID> getAssetsOfType(AssetType type) const;
        [[nodiscard]] DynamicArray<FileID> getAssetsByTag(const String& tag) const;

        void markDirty() { m_dirty = true; }
        [[nodiscard]] Bool isDirty() const { return m_dirty; }
        [[nodiscard]] static UUID generateUUID();
    };

} // dodoe
