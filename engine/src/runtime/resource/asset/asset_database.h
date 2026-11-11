// do@Redlive

#pragma once

#include "dopch.h"

#include "asset.h"
#include "runtime/core/object/object_id.h"

namespace dodoe {

    class AssetDatabase {
        FsPath m_database_path;
        UnorderedMap<ObjectID, AssetMetaData> m_metadata_cache;
        UnorderedMap<String, UUID> m_path_index;
        Bool m_dirty{false};
        mutable std::mutex m_mutex;

    public:
        explicit AssetDatabase(const FsPath& project_asset_dir);
        ~AssetDatabase();

        [[nodiscard]] Bool load();
        Bool save();

        [[nodiscard]] Bool hasAsset(const ObjectID& id) const;
        [[nodiscard]] AssetMetaData DODOE_API getMetaData(const ObjectID& id) const;
        void setMetaData(const ObjectID& id, const AssetMetaData& meta);
        void removeAsset(const ObjectID& id);

        [[nodiscard]] UUID getAssetIdByPath(const String& source_path) const;

        [[nodiscard]] DynamicArray<ObjectID> DODOE_API getAllAssetIDs() const;
        [[nodiscard]] DynamicArray<ObjectID> getAssetsOfType(AssetType type) const;
        [[nodiscard]] DynamicArray<ObjectID> getAssetsByTag(const String& tag) const;

        void markDirty() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_dirty = true;
        }
        [[nodiscard]] Bool isDirty() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_dirty;
        }
        [[nodiscard]] static UUID generateUUID();
    };

} // dodoe
