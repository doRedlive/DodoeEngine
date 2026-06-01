// do@Redlive

#pragma once

#include "dopch.h"

#include "asset.h"
#include "runtime/core/meta/serializer/serializer.h"
#include "runtime/core/thread/task_scheduler.h"

namespace dodoe {
    class World;

    struct AssetManagerCreateInfo {

    };

    class AssetManager : public Managed<AssetManager, AssetManagerCreateInfo> {
        friend class Managed<AssetManager, AssetManagerCreateInfo>;
        std::unordered_map<AssetHandle, AssetRef> m_asset_umap{};
        std::filesystem::path m_asset_dir;
    public:
        bool loadAssets();
        [[nodiscard]] std::vector<AssetRef> getAssetsByType(AssetType type) const;

        template<typename AssetType>
        bool loadAsset(const std::string& asset_url, AssetType& out_asset) const {
            std::filesystem::path asset_path = getFullPath(asset_url);
            std::ifstream asset_json_file(asset_path);
            if (!asset_json_file) {
                DO_ERROR("open file: {} failed!", asset_path.generic_string());
                return false;
            }

            std::stringstream buffer;
            buffer << asset_json_file.rdbuf();
            std::string asset_json_text(buffer.str());

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
        [[nodiscard]] auto loadAssetAsync(const String& asset_url) const
            -> std::future<AssetType> {
            return TaskScheduler::Self().async([this, asset_url]() {
                AssetType asset;
                loadAsset(asset_url, asset);
                return asset;
            });
        }

        template<typename AssetType>
        bool saveAsset(const AssetType& out_asset, const std::string& asset_url) const {
            const std::filesystem::path asset_path = getFullPath(asset_url);
            std::error_code ec;
            std::filesystem::create_directories(asset_path.parent_path(), ec);

            std::ofstream asset_json_file(asset_path);
            if (!asset_json_file) {
                DO_ERROR("open file {} failed!", asset_url);
                return false;
            }

            auto asset_json = Serializer::write(out_asset);
            std::string asset_json_text = asset_json.dump(4);

            asset_json_file << asset_json_text;
            asset_json_file.flush();
            return true;
        }

    private:
        bool initialize(const AssetManagerCreateInfo& info);
        void shutdown();
        [[nodiscard]] std::filesystem::path getFullPath(const std::string& asset_url) const;
    };

} // dodoe
