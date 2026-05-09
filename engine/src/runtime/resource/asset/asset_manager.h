// do@Redlive

#pragma once

#include "dopch.h"

#include "asset.h"

namespace dodoe {

    class AssetManager {
        std::unordered_map<AssetHandle, AssetRef> m_asset_umap{};
    public:
        static Scope<AssetManager> Create();
        static void Destroy(Scope<AssetManager>& manager);

        bool loadAssets();
        [[nodiscard]] std::vector<AssetRef> getAssetsByType(AssetType type) const;

    private:
        bool initialize();
        void shutdown();
    };

} // dodoe

