// do@Redlive

#include "resource_manager.h"

#include "runtime/core/context/system_context.h"
#include "runtime/core/project/project.h"
#include "runtime/resource/file/file_system.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/render/render_service/shared_render_service.h"
#include "runtime/function/render/material/material.h"
#include "runtime/function/render/mesh_draw/mesh.h"
#include "runtime/function/animation/animation.h"
#include "runtime/function/animation/anim_clip.h"
#include "runtime/function/animation/skeleton.h"
#include "runtime/function/animation/animator_controller.h"
#include "runtime/function/render/pixel2d/tileset.h"
#include "runtime/function/render/pixel2d/sprite_manager.h"

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
        m_assetManager->loadAssetSync<SceneAsset>(handle.getObjectID().asset_id);
        return handle;
    }

    Texture2D* ResourceManager::LoadTexture2D(const UUID& asset_id, UInt32 local_id) {
        Asset* asset = m_assetManager->findAsset(asset_id);
        if (!asset) {
            return nullptr;
        }
        auto* texture_manager = GetRenderSystem()->getSharedRenderService()->getTextureManager();
        if (!texture_manager) {
            return nullptr;
        }
        return texture_manager->createTexture(asset->getSourcePath(), ObjectID{asset_id, local_id}, GDrawCommandList, nullptr);
    }

    Sprite* ResourceManager::LoadSprite(const UUID& asset_id, UInt32 local_id) {
        Asset* asset = m_assetManager->findAsset(asset_id);
        if (!asset) {
            return nullptr;
        }
        return SpriteManager::Create(ObjectID{asset_id, local_id}, asset->getSourcePath());
    }

    Material* ResourceManager::LoadMaterial(const UUID& asset_id, UInt32 local_id) {
        Asset* asset = m_assetManager->findAsset(asset_id);
        if (!asset) {
            return nullptr;
        }
        return Material::Create(ObjectID{asset_id, local_id}, asset->getSourcePath());
    }

    Anim2DClip* ResourceManager::LoadAnim2DClip(const UUID& asset_id, UInt32 local_id) {
        Asset* asset = m_assetManager->findAsset(asset_id);
        if (!asset) {
            return nullptr;
        }
        return Anim2DClip::Create(ObjectID{asset_id, local_id}, asset->getSourcePath());
    }

    Skeleton* ResourceManager::LoadSkeleton(const UUID& asset_id, UInt32 local_id) {
        return Skeleton::Create(ObjectID{asset_id, local_id});
    }

    AnimClip* ResourceManager::LoadAnimClip(const UUID& asset_id, UInt32 local_id) {
        return AnimClip::Create(ObjectID{asset_id, local_id});
    }

    AnimatorController* ResourceManager::LoadAnimatorController(const UUID& asset_id, UInt32 local_id) {
        Asset* asset = m_assetManager->findAsset(asset_id);
        if (!asset) {
            return nullptr;
        }
        return AnimatorController::Create(ObjectID{asset_id, local_id}, asset->getSourcePath());
    }

    Tileset* ResourceManager::LoadTileset(const UUID& asset_id, UInt32 local_id) {
        Asset* asset = m_assetManager->findAsset(asset_id);
        if (!asset) {
            return nullptr;
        }
        return Tileset::Create(ObjectID{asset_id, local_id}, asset->getSourcePath());
    }

    Mesh* ResourceManager::LoadMesh(const UUID& asset_id, UInt32 local_id) {
        Asset* asset = m_assetManager->findAsset(asset_id);
        if (!asset) {
            return nullptr;
        }
        return Mesh::Create(ObjectID{asset_id, local_id}, asset->getSourcePath());
    }

} // dodoe
