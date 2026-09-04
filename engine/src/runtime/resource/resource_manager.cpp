// do@Redlive

#include "resource_manager.h"

#include "runtime/core/context/system_context.h"
#include "runtime/core/project/project.h"
#include "runtime/core/utils/common.h"
#include "runtime/resource/file/file_system.h"
#include "runtime/resource/parser/texture_blob.h"
#include "runtime/function/render/render_command.h"
#include "runtime/function/render/material/material.h"
#include "runtime/function/render/mesh_draw/mesh.h"
#include "runtime/function/animation/animation.h"
#include "runtime/function/animation/anim_clip.h"
#include "runtime/function/animation/skeleton.h"
#include "runtime/function/animation/animator_controller.h"
#include "runtime/function/render/pixel2d/tileset.h"
#include "runtime/function/render/pixel2d/sprite_manager.h"
#include "runtime/function/world/prefab.h"
#include "runtime/resource/asset/types/audio_clip_asset.h"

namespace dodoe {

    ResourceManager& ResourceManager::Self() {
        static ResourceManager instance;
        return instance;
    }

    void ResourceManager::initialize(const ResourceManagerInitInfo& info) {
        DO_PROFILE_SCOPE_CATEGORY("ResourceManager::initialize", "startup");
        m_assetManager = AssetManager::Create({});
        SpriteManager::Initialize();
    }

    void ResourceManager::shutdown() {
        DO_PROFILE_SCOPE_CATEGORY("ResourceManager::shutdown", "shutdown");
        SpriteManager::Shutdown();
        Material::Shutdown();
        Mesh::Shutdown();
        Anim2DClip::Shutdown();
        Skeleton::Shutdown();
        AnimClip::Shutdown();
        AnimatorController::Shutdown();
        Tileset::Shutdown();
        Prefab::Shutdown();
        AssetManager::Destroy(m_assetManager);
    }

    Bool ResourceManager::loadAssets() {
        DO_PROFILE_SCOPE_CATEGORY("ResourceManager::loadAssets", "startup");
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
        (void)m_assetManager->loadAssetSync<SceneAsset>(handle.getObjectID().asset_id);
        return handle;
    }

    Texture2D* ResourceManager::loadTexture2D(const UUID& asset_id, UInt32 local_id) {
        Asset* asset = m_assetManager->findAsset(asset_id);
        if (!asset) {
            return nullptr;
        }
        const String& path = asset->getSourcePath();
        const FsPath base = m_assetManager->getAssetDir();
        const auto absolute_path = FileSystem::RelativeToAbsolute(path, base);
        TextureBlob data(absolute_path);
        if (!data.isValid()) {
            DO_ERROR("ResourceManager: Load texture {0} failed!", path);
            return nullptr;
        }

        const Size_t bytes_per_channel = data.is_hdr ? sizeof(Float) : sizeof(UByte);
        const Size_t data_size = static_cast<Size_t>(data.width) * static_cast<Size_t>(data.height) * 4u * bytes_per_channel;

        auto texture = create_scope<Texture2D>(ObjectID{asset_id, local_id});
        Texture2D* texture_raw = texture.get();
        texture->setDimensions(data.width, data.height);
        texture->setPath(path);

        ResourceCommand cmd;
        cmd.type = ResourceCommandType::CreateTexture;
        cmd.texture_object = std::move(texture);
        cmd.texture_is_hdr = data.is_hdr;
        if (data.pixels && data_size > 0) {
            cmd.resource_data.assign(static_cast<const UInt8*>(data.pixels), static_cast<const UInt8*>(data.pixels) + data_size);
        }
        if (auto* rs = GetRenderSystem()) {
            rs->enqueueResourceCommand(std::move(cmd));
        }
        return texture_raw;
    }

    Sprite* ResourceManager::loadSprite(const UUID& asset_id, UInt32 local_id) {
        Asset* asset = m_assetManager->findAsset(asset_id);
        if (!asset) {
            return nullptr;
        }
        return SpriteManager::Create(ObjectID{asset_id, local_id}, asset->getSourcePath());
    }

    Material* ResourceManager::loadMaterial(const UUID& asset_id, UInt32 local_id) {
        Asset* asset = m_assetManager->findAsset(asset_id);
        if (!asset) {
            return nullptr;
        }
        return Material::Create(ObjectID{asset_id, local_id}, asset->getSourcePath());
    }

    Anim2DClip* ResourceManager::loadAnim2DClip(const UUID& asset_id, UInt32 local_id) {
        Asset* asset = m_assetManager->findAsset(asset_id);
        if (!asset) {
            return nullptr;
        }
        return Anim2DClip::Create(ObjectID{asset_id, local_id}, asset->getSourcePath());
    }

    Skeleton* ResourceManager::loadSkeleton(const UUID& asset_id, UInt32 local_id) {
        return Skeleton::Create(ObjectID{asset_id, local_id});
    }

    AnimClip* ResourceManager::loadAnimClip(const UUID& asset_id, UInt32 local_id) {
        return AnimClip::Create(ObjectID{asset_id, local_id});
    }

    AnimatorController* ResourceManager::loadAnimatorController(const UUID& asset_id, UInt32 local_id) {
        Asset* asset = m_assetManager->findAsset(asset_id);
        if (!asset) {
            return nullptr;
        }
        return AnimatorController::Create(ObjectID{asset_id, local_id}, asset->getSourcePath());
    }

    Tileset* ResourceManager::loadTileset(const UUID& asset_id, UInt32 local_id) {
        Asset* asset = m_assetManager->findAsset(asset_id);
        if (!asset) {
            return nullptr;
        }
        return Tileset::Create(ObjectID{asset_id, local_id}, asset->getSourcePath());
    }

    Mesh* ResourceManager::loadMesh(const UUID& asset_id, UInt32 local_id) {
        Asset* asset = m_assetManager->findAsset(asset_id);
        if (!asset) {
            return nullptr;
        }
        return Mesh::Create(ObjectID{asset_id, local_id}, asset->getSourcePath());
    }

    AudioClip* ResourceManager::loadAudioClip(const UUID& asset_id, UInt32 local_id) {
        Asset* asset = m_assetManager->findAsset(asset_id);
        if (!asset || asset->getType() != AssetType::Audio) {
            return nullptr;
        }
        auto* clip_asset = static_cast<AudioClipAsset*>(asset);
        return clip_asset->getClip();
    }

    Prefab* ResourceManager::loadPrefab(const UUID& asset_id, UInt32 local_id) {
        Asset* asset = m_assetManager->findAsset(asset_id);
        if (!asset) {
            return nullptr;
        }
        return Prefab::Create(ObjectID{asset_id, local_id}, asset->getSourcePath());
    }

} // dodoe
