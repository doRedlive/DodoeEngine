// do@Redlive

#pragma once

#include "dopch.h"

#include "asset.h"
#include "asset_handle.h"
#include "asset_database.h"
#include "types/texture_asset.h"
#include "types/sprite_asset.h"
#include "types/mesh_asset.h"
#include "types/material_asset.h"
#include "types/anim2d_clip_asset.h"
#include "types/scene_asset.h"
#include "types/input_action_asset.h"
#include "types/audio_clip_asset.h"

#include "runtime/core/meta/serializer/serializer.h"
#include "runtime/core/async/task_scheduler.h"
#include "runtime/core/object/object_id.h"
#include "runtime/resource/file/file_id.h"

#include <shared_mutex>

namespace dodoe {

    struct AssetManagerCreateInfo {};

    class AssetManager : public Managed<AssetManager, AssetManagerCreateInfo> {
        friend class Managed<AssetManager, AssetManagerCreateInfo>;

        Scope<AssetDatabase> m_database;
        UnorderedMap<UUID, Scope<Asset>> m_assets;
        UnorderedMap<String, UUID> m_path_to_asset_id;
        DynamicArray<UUID> m_assets_by_type[static_cast<Size_t>(AssetType::Count)];
        FsPath m_asset_dir;
        mutable std::shared_mutex m_mutex;

        Bool initialize(const AssetManagerCreateInfo& info);
        void shutdown();

        [[nodiscard]] FsPath getFullPath(const String& asset_url) const;

        void importSourceFile(const FsPath& absolute_path, const String& source_path, const String& ext);
        static void EnsureBuiltinImporters();

    public:
        [[nodiscard]] AssetDatabase* getDatabase() const { return m_database.get(); }
        [[nodiscard]] const FsPath& getAssetDir() const { return m_asset_dir; }

        UUID registerAsset(const String& source_path, AssetType type);
        Scope<Asset> createAssetInstance(AssetType type);
        void registerMaterialAsset(Scope<MaterialAsset> asset);

        [[nodiscard]] Asset* findAsset(const UUID& asset_id) const;

        template<typename T>
        [[nodiscard]] T* findAsset(const UUID& asset_id) const {
            Asset* asset = findAsset(asset_id);
            if (asset && asset->getType() == T::kStaticType) {
                return static_cast<T*>(asset);
            }
            return nullptr;
        }

        [[nodiscard]] Asset* findAssetByPath(const String& source_path) const;

        [[nodiscard]] ObjectID DODOE_API resolvePathToRef(const FileID& file_id) const;
        [[nodiscard]] ObjectID resolveSubObjectRef(const FileID& file_id, UInt32 local_id) const;

        [[nodiscard]] ObjectID DODOE_API ensureImported(const String& absolute_path);
        [[nodiscard]] ObjectID DODOE_API ensureTilesetImported(const String& absolute_path);

        template<typename T>
        [[nodiscard]] AssetHandle<T> getHandle(const UUID& asset_id) const {
            return AssetHandle<T>(ObjectID{asset_id, 0});
        }

        template<typename T>
        [[nodiscard]] AssetHandle<T> getHandleByPath(const String& source_path) const {
            std::shared_lock lock(m_mutex);
            auto it = m_path_to_asset_id.find(source_path);
            if (it != m_path_to_asset_id.end()) {
                return AssetHandle<T>(ObjectID{it->second, 0});
            }
            return AssetHandle<T>();
        }

        template<typename T>
        [[nodiscard]] T* loadAssetSync(const UUID& asset_id) {
            T* asset = findAsset<T>(asset_id);
            if (!asset) {
                return nullptr;
            }
            if (asset->isLoaded()) {
                return asset;
            }
            if (asset->getLoadState() == AssetLoadState::Loading) {
                return nullptr;
            }
            asset->setLoadState(AssetLoadState::Loading);
            String absolute_path((m_asset_dir / asset->getSourcePath()).string().c_str());
            if (!asset->loadFromSource(absolute_path)) {
                asset->setLoadState(AssetLoadState::Failed);
                return nullptr;
            }
            asset->setLoadState(AssetLoadState::Loaded);
            return asset;
        }

        template<typename T>
        [[nodiscard]] auto loadAssetAsync(const UUID& asset_id) -> std::future<T*> {
            return TaskScheduler::Self().async([this, asset_id]() -> T* {
                return loadAssetSync<T>(asset_id);
            });
        }

        void unloadAsset(const UUID& asset_id);
        void unloadAll();
        Bool DODOE_API saveAsset(const UUID& asset_id) const;

        Bool loadAssets();
        [[nodiscard]] auto loadAssetsAsync() const -> std::future<void>;
        void discoverAssets();

        [[nodiscard]] Bool DODOE_API isAssetDirty(const UUID& asset_id) const;
        Bool DODOE_API reimportAsset(const UUID& asset_id);
        Bool DODOE_API refreshAssets();

        template<typename T>
        [[nodiscard]] DynamicArray<AssetHandle<T>> getAssets() const {
            return getAllAssetsOfType<T>();
        }

        template<typename T>
        [[nodiscard]] DynamicArray<AssetHandle<T>> getAllAssetsOfType() const {
            std::shared_lock lock(m_mutex);
            const Size_t type_idx = static_cast<Size_t>(T::kStaticType);
            DynamicArray<AssetHandle<T>> result;
            result.reserve(m_assets_by_type[type_idx].size());
            for (const auto& asset_id : m_assets_by_type[type_idx]) {
                result.emplace_back(ObjectID{asset_id, 0});
            }
            return result;
        }

        [[nodiscard]] Size_t getAssetCount() const;
        [[nodiscard]] Size_t getAssetCountOfType(AssetType type) const;

        [[nodiscard]] DynamicArray<UUID> getDependents(const UUID& asset_id) const;
        [[nodiscard]] DynamicArray<ObjectID> getDependencies(const UUID& asset_id) const;

        [[nodiscard]] String getAssetPath(const UUID& asset_id) const;

        template<typename AssetType>
        Bool loadAssetFile(const String& asset_url, AssetType& out_asset) const {
            FsPath asset_path = getFullPath(asset_url);
            std::ifstream asset_json_file(asset_path);
            if (!asset_json_file) {
                DO_ERROR("open file: {} failed!", asset_path.generic_string());
                return false;
            }

            std::stringstream buffer;
            buffer << asset_json_file.rdbuf();
            String asset_json_text(buffer.str());

            Json asset_json;
            try {
                asset_json = Json::parse(asset_json_text);
            }
            catch (const Json::exception&) {
                DO_ERROR("parse json file {} failed!", asset_url);
                return false;
            }

            Serializer::read(asset_json, out_asset);
            return true;
        }

        template<typename AssetType>
        [[nodiscard]] auto loadAssetFileAsync(const String& asset_url) const
            -> std::future<AssetType> {
            return TaskScheduler::Self().async([this, asset_url]() {
                AssetType asset;
                loadAssetFile(asset_url, asset);
                return asset;
            });
        }

        template<typename AssetType>
        Bool saveAssetFile(const AssetType& out_asset, const String& asset_url) const {
            const FsPath asset_path = getFullPath(asset_url);
            std::error_code ec;
            std::filesystem::create_directories(asset_path.parent_path(), ec);

            std::ofstream asset_json_file(asset_path);
            if (!asset_json_file) {
                DO_ERROR("open file {} failed!", asset_url);
                return false;
            }

            auto asset_json = Serializer::write(out_asset);
            String asset_json_text(asset_json.dump(4).c_str());

            asset_json_file << asset_json_text;
            asset_json_file.flush();
            return true;
        }
    };

} // dodoe
