//
// Created by GreenMuffin on 2025/10/28.
//

#include "resource_manager.h"

#include "runtime/resource/file/file_system.h"

namespace dodoe {

    ResourceManager& ResourceManager::Self() {
        static ResourceManager instance;
        return instance;
    }

    void ResourceManager::initialize() {
        m_assetManager = AssetManager::Create({});
    }

    void ResourceManager::shutdown() {
        AssetManager::Destroy(m_assetManager);
    }

    Bool ResourceManager::loadAssets() {
        return m_assetManager->loadAssets();
    }

    AssetHandle<TextureAsset> ResourceManager::getTexture(const String& path) {
        AssetHandle<TextureAsset> handle = m_assetManager->getHandleByPath<TextureAsset>(path);
        if (!handle.isValid()) {
            String normalized = FileSystem::NormalizePath(path);
            handle = m_assetManager->getHandleByPath<TextureAsset>(normalized);
        }
        return handle;
    }

    AssetHandle<MeshAsset> ResourceManager::getMesh(const String& path) {
        AssetHandle<MeshAsset> handle = m_assetManager->getHandleByPath<MeshAsset>(path);
        if (!handle.isValid()) {
            String normalized = FileSystem::NormalizePath(path);
            handle = m_assetManager->getHandleByPath<MeshAsset>(normalized);
        }
        return handle;
    }

    AssetHandle<SceneAsset> ResourceManager::loadScene(const String& path) {
        AssetHandle<SceneAsset> handle = m_assetManager->getHandleByPath<SceneAsset>(path);
        if (!handle.isValid()) {
            return AssetHandle<SceneAsset>();
        }
        m_assetManager->loadAssetSync<SceneAsset>(handle.getFileID());
        return handle;
    }

} // dodoe
