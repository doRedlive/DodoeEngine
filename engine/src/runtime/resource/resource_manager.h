// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/utils/util.h"
#include "asset/asset_manager.h"

namespace dodoe {

    struct ResourceManagerInitInfo {
        std::filesystem::path project_path;
    };

    class ResourceManager {
    public:
        static ResourceManager& Self();

        ResourceManager(const ResourceManager&) = delete;
        ResourceManager& operator=(const ResourceManager&) = delete;
        ResourceManager(ResourceManager&&) = delete;
        ResourceManager& operator=(ResourceManager&&) = delete;

        void initialize(const ResourceManagerInitInfo& info);
        void shutdown();

        [[nodiscard]] AssetManager* getAssetManager() const { return m_assetManager.get(); }

        Bool loadAssets();
        [[nodiscard]] auto loadAssetsAsync() const { return m_assetManager->loadAssetsAsync(); }

        template<typename T>
        [[nodiscard]] DynamicArray<AssetHandle<T>> getAssets() const {
            return m_assetManager->getAssets<T>();
        }

        [[nodiscard]] AssetHandle<TextureAsset> getTexture(const String& path);
        [[nodiscard]] AssetHandle<MeshAsset> getMesh(const String& path);
        [[nodiscard]] AssetHandle<SceneAsset> loadScene(const String& name);

    private:
        ResourceManager() = default;
        Scope<AssetManager> m_assetManager{nullptr};
    };

    template<typename T>
    T* AssetHandle<T>::get() const {
        if (!m_file_id.isValid()) {
            return nullptr;
        }
        AssetManager* manager = ResourceManager::Self().getAssetManager();
        if (!manager) {
            return nullptr;
        }
        Asset* asset = manager->findAsset(m_file_id);
        if (asset) {
            return static_cast<T*>(asset);
        }
        return nullptr;
    }

    template<typename T>
    T* AssetHandle<T>::operator->() const {
        return get();
    }

    template<typename T>
    T& AssetHandle<T>::operator*() const {
        return *get();
    }

    template<typename T>
    Bool AssetHandle<T>::isLoaded() const {
        T* asset = get();
        return asset && asset->isLoaded();
    }

} // dodoe
