// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/utils/util.h"
#include "runtime/core/object/object.h"
#include "asset/asset_manager.h"

namespace dodoe {

    class Texture2D;
    class Sprite;
    class Material;
    class Anim2DClip;
    class Skeleton;
    class AnimClip;
    class AnimatorController;
    class Tileset;
    class Mesh;

    namespace detail {

        template<typename...>
        struct always_false { static constexpr bool value = false; };

        template<typename T>
        struct ObjectTypeName;
        template<>
        struct ObjectTypeName<Texture2D> { static constexpr const char* kValue = "Texture2D"; };
        template<>
        struct ObjectTypeName<Sprite> { static constexpr const char* kValue = "Sprite"; };
        template<>
        struct ObjectTypeName<Material> { static constexpr const char* kValue = "Material"; };
        template<>
        struct ObjectTypeName<Anim2DClip> { static constexpr const char* kValue = "Anim2DClip"; };
        template<>
        struct ObjectTypeName<Skeleton> { static constexpr const char* kValue = "Skeleton"; };
        template<>
        struct ObjectTypeName<AnimClip> { static constexpr const char* kValue = "AnimClip"; };
        template<>
        struct ObjectTypeName<AnimatorController> { static constexpr const char* kValue = "AnimatorController"; };
        template<>
        struct ObjectTypeName<Tileset> { static constexpr const char* kValue = "Tileset"; };
        template<>
        struct ObjectTypeName<Mesh> { static constexpr const char* kValue = "Mesh"; };

    } // namespace detail

    struct ResourceManagerInitInfo {
        FsPath project_path;
    };

    class ResourceManager {
    public:
        static DODOE_API ResourceManager& Self();

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

        template<typename T>
        [[nodiscard]] T* findLoaded(const UUID& asset_id, UInt32 local_id) const {
            const InstanceID inst = Object::FindInstanceID(ObjectID{asset_id, local_id});
            if (inst == 0) {
                return nullptr;
            }
            Object* obj = Object::FindObjectFromInstanceID(inst);
            if (!obj || String(obj->getObjectTypeName()) != String(detail::ObjectTypeName<T>::kValue)) {
                return nullptr;
            }
            return static_cast<T*>(obj);
        }

        template<typename T>
        [[nodiscard]] T* loadObject(const UUID& asset_id, UInt32 local_id) {
            if (T* loaded = findLoaded<T>(asset_id, local_id)) {
                return loaded;
            }
            if constexpr (std::is_same_v<T, Texture2D>) {
                return LoadTexture2D(asset_id, local_id);
            } else if constexpr (std::is_same_v<T, Sprite>) {
                return LoadSprite(asset_id, local_id);
            } else if constexpr (std::is_same_v<T, Material>) {
                return LoadMaterial(asset_id, local_id);
            } else if constexpr (std::is_same_v<T, Anim2DClip>) {
                return LoadAnim2DClip(asset_id, local_id);
            } else if constexpr (std::is_same_v<T, Skeleton>) {
                return LoadSkeleton(asset_id, local_id);
            } else if constexpr (std::is_same_v<T, AnimClip>) {
                return LoadAnimClip(asset_id, local_id);
            } else if constexpr (std::is_same_v<T, AnimatorController>) {
                return LoadAnimatorController(asset_id, local_id);
            } else if constexpr (std::is_same_v<T, Tileset>) {
                return LoadTileset(asset_id, local_id);
            } else if constexpr (std::is_same_v<T, Mesh>) {
                return LoadMesh(asset_id, local_id);
            } else {
                static_assert(detail::always_false<T>::value, "ResourceManager::loadObject: unsupported type");
            }
        }

        template<typename T>
        [[nodiscard]] T* loadObjectByPath(const FileID& file_id) {
            if (!file_id.isValid()) {
                return nullptr;
            }
            ObjectID ref;
            if constexpr (std::is_same_v<T, Sprite>) {
                ref = m_assetManager->resolveSubObjectRef(file_id, 0);
            } else {
                ref = m_assetManager->resolvePathToRef(file_id);
            }
            if (!ref.isValid()) {
                return nullptr;
            }
            return loadObject<T>(ref.asset_id, ref.local_id);
        }

        [[nodiscard]] AssetHandle<TextureAsset> getTexture(const String& path);
        [[nodiscard]] AssetHandle<MeshAsset> getMesh(const String& path);
        [[nodiscard]] AssetHandle<SceneAsset> loadScene(const String& name);

    private:
        ResourceManager() = default;
        Scope<AssetManager> m_assetManager{nullptr};

        Texture2D* LoadTexture2D(const UUID& asset_id, UInt32 local_id);
        Sprite* LoadSprite(const UUID& asset_id, UInt32 local_id);
        Material* LoadMaterial(const UUID& asset_id, UInt32 local_id);
        Anim2DClip* LoadAnim2DClip(const UUID& asset_id, UInt32 local_id);
        Skeleton* LoadSkeleton(const UUID& asset_id, UInt32 local_id);
        AnimClip* LoadAnimClip(const UUID& asset_id, UInt32 local_id);
        AnimatorController* LoadAnimatorController(const UUID& asset_id, UInt32 local_id);
        Tileset* LoadTileset(const UUID& asset_id, UInt32 local_id);
        Mesh* LoadMesh(const UUID& asset_id, UInt32 local_id);
    };

    template<typename T>
    T* AssetHandle<T>::get() const {
        if (!m_id.isValid()) {
            return nullptr;
        }
        AssetManager* manager = ResourceManager::Self().getAssetManager();
        if (!manager) {
            return nullptr;
        }
        Asset* asset = manager->findAsset(m_id.asset_id);
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
